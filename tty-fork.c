#define _XOPEN_SOURCE // Allow us to fully access the necessary PTY functions.

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "watch.h"
#include "util.h"

// Maintain these variables globally so that we can gracefully exit when
// we receive SIGCHLD. Otherwise, a FORCE'd invariant could trigger before
// the cleanup code is reached, causing the code to apparently exit with
// an error.
const char *socket_path = NULL;
int         socket_fd   = -1;
int         pty_fd      = -1;

void cleanup()
{
    close(pty_fd);
    close(socket_fd);    // Ensure we can unlink the socket.
    unlink(socket_path); // Prevent invalid dangling files.
}

void sigexit(int s)
{
    exit(EXIT_SUCCESS);
}

int make_domain_socket(const char *path)
{
    int sock, retval;
    struct sockaddr_un addr;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    FORCE(sock != -1, "Unable to create socket for IPC.");

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path));
    retval = bind(sock, (struct sockaddr *)&addr, strlen(path) + sizeof(addr.sun_family));
    FORCE(!retval, "Unable to bind the IPC socket to the filesystem.");

    FORCE(!listen(sock, 5), "Unable to listen on socket.");

    return sock;
}

int forkpty(int argc, char **args)
{
    int pty;
    
    FORCE(isatty(0) && isatty(1), "Must call tty-fork from a valid tty.");
    
    pty = posix_openpt(O_RDWR | O_NOCTTY);
    FORCE(pty != -1, "Unable to open a new pseudo-terminal.");
    
    FORCE(!grantpt(pty) && !unlockpt(pty), "Unable to release pseudo-terminal.");
  
    // XXX: Catch errors when forking. 
    if(fork() == 0) {
        // This is the child process. Clone the arguments and call execvp.
        char **argv = malloc(sizeof(char*) * (argc + 1));
        char *ptyname;
        int pgroup;
        struct termios tc;

        FORCE(argv != NULL, "Unable to allocate memory.");
        memmove(argv, args, sizeof(char*) * argc);
        argv[argc] = NULL; // Terminate the vector of arguments.

        ptyname = ptsname(pty); 
        FORCE(ptyname != NULL, "Could not open a pseudo-terminal slave.");
        close(pty);

        pty = open(ptyname, O_RDWR);
        FORCE(pty, "Could not access the terminal slave.");
        
        dup2(pty, STDIN_FILENO); // Replace stdin with the pseudo-tty.

//        pgroup = setsid();
//        FORCE(pgroup > -1, "Could not create a new session.");
//        FORCE(tcsetpgrp(0, pgroup) != -1, "Could not set the process to the foreground.");

        tc.c_lflag = ISIG | ICANON | ECHOE | ECHOK | /*ECHOCTL | ECHOKE |*/ IEXTEN;
        tc.c_oflag = TABDLY | OPOST;
        tc.c_iflag = BRKINT | IGNPAR | ISTRIP | ICRNL | IXON | IMAXBEL;
        tc.c_cflag = /*CBAUD |*/ CS8 | CREAD;
        tc.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &tc);
         
        execvp(argv[0], argv);
        FORCE(0, "Unable to execute slave process.");
    } else {
        signal(SIGCHLD, sigexit);
        signal(SIGABRT, sigexit);
        signal(SIGKILL, sigexit);
        signal(SIGTERM, sigexit);
        signal(SIGSEGV, sigexit);
        signal(SIGILL,  sigexit);
        signal(SIGINT,  sigexit);
    }

    return pty;
}

// The length of the buffer to read data into.
#define READ_BUFFER_LEN 2048

void select_loop(int pty, int sock)
{
    int count, new_fd;
    unsigned int i;
    size_t len;
    struct watched_fds *watcher = new_watcher();
    char read_buffer[READ_BUFFER_LEN];

    watch_fd(watcher, STDIN_FILENO);
    watch_fd(watcher, pty);
    watch_fd(watcher, sock);

    while ((count = watch_for_data(watcher))) {
        FORCE(!FD_ISSET(sock, &watcher->error_set), "Server connection lost.");
        FORCE(!FD_ISSET(pty, &watcher->error_set),  "Terminal connection lost.");
        FORCE(!FD_ISSET(pty, &watcher->read_set),  "Terminal connection with data!?");
        
        if (FD_ISSET(sock, &watcher->read_set)) {
            new_fd = accept(sock, NULL, NULL);
            watch_fd(watcher, new_fd);
            FD_CLR(sock, &watcher->read_set);
            count--;
        }

        for (i = 0; i < watcher->len && count > 0; i++) {
            if (FD_ISSET(watcher->fds[i], &watcher->error_set))
                unwatch_fd(watcher, watcher->fds[i]);
            if (FD_ISSET(watcher->fds[i], &watcher->read_set)) {
                // XXX: Handle EOF gracefully (send it and continue working.)
                len = read(watcher->fds[i], read_buffer, READ_BUFFER_LEN);
                FORCE(len > 0, "Failed read from socket.");
                len = write(pty, read_buffer, len);
                FORCE(len > 0, "Failed write to terminal.");
            }
        }
    }
}

const char *USAGE = "Usage: tty-fork <path> <command> [arguments] ...";

int main(int argc, char **argv)
{
    if (argc < 2) {
        puts(USAGE);
        exit(1);
    }

    socket_path = argv[1];
    socket_fd   = make_domain_socket(argv[1]);
    pty_fd      = forkpty(argc - 2, argv + 2); // Skip the command and socket path.

    atexit(cleanup);
     
    select_loop(pty_fd, socket_fd);
    // XXX: Should not be reached.

    exit(EXIT_SUCCESS);
}
