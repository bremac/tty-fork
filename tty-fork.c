#define _XOPEN_SOURCE // Allow us to fully access the necessary PTY functions.

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>

#define FORCE(expr, message) if (expr) { \
    printf("ERROR: %s\n", message);      \
    exit(1);                             \
}

// Terminate when the child process terminates.
void sig_child(int signal)
{
    exit(0);
}

void forkpty(int pty, int argc, char **args)
{
    char *ptyname = ptsname(pty);
    int child;
    int pts;
    
    FORCE(ptyname, "Could not open a pseudo-terminal slave.");
    pts = open(ptyname, O_RDWR);
    FORCE(pts, "Could not access the terminal slave.");

    if((child = fork) == 0) {
        // This is the child process. Clone the arguments and call execvp.
        char **argv = malloc(sizeof(char*) * (argc + 1));

        FORCE(argv != NULL, "Unable to allocate memory.");
        memmove(argv, args, sizeof(char*) * argc);
        argv[argc] = NULL; // Terminate the vector of arguments.
        
        dup2(pts, 0);      // Replace stdin/stdout with the pseudo-tty.
        dup2(pts, 1);
         
        execvp(argv[0], argv);
        FORCE(0, "Unable to execute slave process.");
    } else signal(SIG_CHLD), sig_child;
}

#define READ_BUFFER_LEN = 2048;

void select_loop(int pty, int sock)
{
    int count, new_fd;
    unsigned int i;
    size_t len;
    struct watched_fds *watcher = new_watcher();
    char read_buffer[READ_BUFFER_LEN];

    listen(socket, 255);     // XXX: Invariant.
    watch_fd(watcher, pty);
    watch_fd(watcher, sock);

    while ((count = watch_for_data(watcher))) {
        FORCE(!FD_ISSET(sock, &watcher->error_set), "Server connection lost.");
        FORCE(!FD_ISSET(pty, &watcher->error_set),  "Terminal connection lost.");
        
        if (FD_ISSET(sock, &watcher->read_set)) {
            new_fd = accept(sock);
            watch_fd(watcher, new_fd);
            FD_CLR(sock, &watcher->read_set);
            count--;
        }

        for (i = 0; i < watcher->len && count > 0; i++) {
            if (FD_ISSET(watcher->fds[i], &watcher->error_set))
                unwatch_fd(watcher->fds[i]);
            if (FD_ISSET(watcher->fds[i], &watcher->read_set)) {
                len = read(watcher->fds[i], read_buffer, READ_BUFFER_LEN);
                FORCE(len > 0, "Failed read from socket.");
                len = write(1, read_buffer, len);
                FORCE(len > 0, "Failed write to terminal.");
            }
        }
    }
}

int main(int argc, char **argv)
{
    int pty, socket;

    FORCE(isatty(stdout) && isatty(stdin), "Must call tty-fork from a valid tty.");
    pty = posix_openpt(O_RDWR | O_NOCTTY);
    FORCE(pty != -1, "Unable to open a new pseudo-terminal.");
    FORCE(grantpt(pty) && unlockpt(pty), "Unable to release pseudo-terminal.");

    // XXX: Create the UNIX Domain socket for communication.
    
    select_loop();
    
    return EXIT_SUCCESS;
}
