#define _XOPEN_SOURCE 600

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
// the cleanup code is reached, causing the domain sockets to remain on the
// filesystem, and terminal settings to remain incorrect.
const char *socket_path = NULL;
int         socket_fd   = -1;
int         pty_fd      = -1;
struct termios tty_orig;

void cleanup()
{
    close(pty_fd);
    close(socket_fd);    // Ensure we can unlink the socket.
    unlink(socket_path); // Prevent invalid dangling files.
    tcsetattr(STDOUT_FILENO, TCSANOW, &tty_orig); // Reset the tty.
    puts("");            // Insert a newline to make things neater.
}

void sigexit(int s)
{
    exit(EXIT_SUCCESS);
}

int make_domain_server(const char *path)
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
    int child;
    struct termios tc;
    
    FORCE(isatty(0) && isatty(1), "Must call tty-fork from a valid tty.");
    
    pty = posix_openpt(O_RDWR | O_NOCTTY);
    FORCE(pty != -1, "Unable to open a new pseudo-terminal.");
    
    FORCE(!grantpt(pty) && !unlockpt(pty), "Unable to release pseudo-terminal.");

    FORCE((child = fork()) != -1, "Unable to fork the child process.");

    if (child == 0) {
        // This is the child process. Clone the arguments and call execvp.
        char **argv = malloc(sizeof(char*) * (argc + 1));
        char *ptyname;

        FORCE(argv != NULL, "Unable to allocate memory.");
        memmove(argv, args, sizeof(char*) * argc);
        argv[argc] = NULL; // Null-terminate the vector of arguments.

        ptyname = ptsname(pty); 
        FORCE(ptyname != NULL, "Could not open a pseudo-terminal slave.");
        close(pty);
        
        // Give the child process control. We need to do this before opening
        // the terminal so that it becomes the controlling terminal.
        FORCE(setsid() != -1, "Unable to create a new session.");

        pty = open(ptyname, O_RDWR);
        FORCE(pty, "Could not access the terminal slave.");
        
        // Make the pseudo-terminal mimic what most slave programs assume to
        // be the default; those requiring more control will change these
        // flags themselves.
        tcgetattr(pty, &tc);
        tc.c_iflag |= (IXON|INLCR|ICRNL);
        tc.c_lflag |= (ECHO|ECHOE|ECHOK|ISIG|IEXTEN|ICANON);
        tc.c_oflag |= (OPOST|OCRNL);
        tcsetattr(pty, TCSANOW, &tc);
        
        // Replace std fds with the pseudo-tty.
        dup2(pty, STDIN_FILENO);
        dup2(pty, STDOUT_FILENO);
        dup2(pty, STDERR_FILENO);

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

    // Save the original tty settings.
    tcgetattr(STDOUT_FILENO, &tty_orig);
    
    // Put our TTY in raw mode: (We will manually convert NL to CRNL.)
    tcgetattr(STDOUT_FILENO, &tc);
    tc.c_iflag = 0;
    tc.c_lflag = 0;
    tc.c_oflag = 0;
    tcsetattr(STDOUT_FILENO, TCSANOW, &tc);

    tcgetattr(pty, &tc);
    tc.c_iflag = 0;
    tc.c_lflag = 0;
    tc.c_oflag = 0;
    tcsetattr(pty, TCSANOW, &tc);

    return pty;
}

void select_loop(int pty, int sock)
{
    int count, new_fd;
    unsigned int i;
    struct watched_fds *watcher = new_watcher();

    watch_fd(watcher, STDIN_FILENO);
    watch_fd(watcher, pty);
    watch_fd(watcher, sock);

    while ((count = watch_for_data(watcher))) {
#define UNFLAG(fd)                   \
    FD_CLR(fd, &watcher->error_set); \
    FD_CLR(fd, &watcher->read_set);  \
    count--;
         
        FORCE(!FD_ISSET(sock, &watcher->error_set), "Server connection lost.");
        FORCE(!FD_ISSET(pty, &watcher->error_set),  "Terminal connection lost.");
        if(FD_ISSET(pty, &watcher->read_set)) {
            transfer_mapped(write_crnl, pty, STDOUT_FILENO);

            // Sometimes the tty gets flagged for reading when the child exits,
            // but the read fails (ex. python.) Fail silently, because thus
            // far it hasn't occurred in an actual erroneous case, and just
            // prints noise to the terminal.

            UNFLAG(pty);
        }

        if (FD_ISSET(sock, &watcher->read_set)) {
            new_fd = accept(sock, NULL, NULL);
            FORCE(new_fd != -1, "Unable to accept IPC connections.");
            watch_fd(watcher, new_fd);
            UNFLAG(sock);
        }

        for (i = 0; i < watcher->len && count > 0; i++) {
            if (FD_ISSET(watcher->fds[i], &watcher->error_set)) {
                unwatch_fd(watcher, watcher->fds[i]);
                UNFLAG(watcher->fds[i]);
            }
            
            if (FD_ISSET(watcher->fds[i], &watcher->read_set)) {
                // Incoming newlines from the Unix Domain sockets need to be
                // transformed into carriage returns for processing by the pty.
                int ret = transfer_mapped(write_cr, watcher->fds[i], pty);

                FORCE(ret != -1, "Unable to transfer IO.");
                
                if (!ret) { // We have reached end-of-file.
                    close(watcher->fds[i]);
                    unwatch_fd(watcher, watcher->fds[i]);

                    // If the only fds left are the master socket and the
                    // pseudo-terminal, then we're done.
                    if (watcher->len == 2) exit(EXIT_SUCCESS);
                }
                UNFLAG(watcher->fds[i]);
            }
        }
#undef UNFLAG
    }
}

const char *USAGE = "Usage: tty-fork <path> <command> [arguments] ...";

int main(int argc, char **argv)
{
    if (argc < 3) {
        puts(USAGE);
        exit(1);
    }

    socket_path = argv[1];
    socket_fd   = make_domain_server(argv[1]);
    pty_fd      = forkpty(argc - 2, argv + 2); // Skip the command and socket path.

    atexit(cleanup);
     
    select_loop(pty_fd, socket_fd);

    exit(EXIT_SUCCESS);
}
