#define _XOPEN_SOURCE 600

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/socket.h>
#include "error.h"
#include "util.h"
#include "watch.h"

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

// XXX: Check for failure of tc(g|s)setattr.
// XXX: Properly set the character correspondences (ie. ERASE) on the slave.
// XXX: Consider using ioctl to set the window size of the slave.

void set_tty_raw(int fd)
{
    struct termios tc;

    tcgetattr(fd, &tc);
    tc.c_iflag = 0;
    tc.c_lflag = 0;
    tc.c_oflag = 0;
    tcsetattr(fd, TCSANOW, &tc);
}

int forkpty(int argc, char **args)
{
    int pty;
    int child;
    
    FORCE(isatty(0) && isatty(1), ERR_NO_TTY);
    
    pty = posix_openpt(O_RDWR | O_NOCTTY);
    FORCE(pty != -1, ERR_NO_PTY);
    
    FORCE(!grantpt(pty) && !unlockpt(pty), ERR_NO_PTY);

    FORCE((child = fork()) != -1, ERR_NO_FORK);

    if (child == 0) {
        // Clone the arguments and call execvp.
        struct termios tc;
        char *ptyname;
        char **argv = malloc(sizeof(char*) * (argc + 1));

        FORCE(argv != NULL, ERR_NO_MEMORY);
        memmove(argv, args, sizeof(char*) * argc);
        argv[argc] = NULL; // Null-terminate the vector of arguments.

        ptyname = ptsname(pty); 
        FORCE(ptyname != NULL, ERR_NO_PTY); // What happens to the parent here?
        close(pty);
        
        // Give the child process control. We need to do this before opening
        // the terminal so that it becomes the controlling terminal.
        FORCE(setsid() != -1, ERR_NO_SESSION);

        pty = open(ptyname, O_RDWR);
        FORCE(pty, ERR_NO_PTY);
        
        // Make the pseudo-terminal mimic what most slave programs assume to
        // be the default; those requiring more control will change these
        // flags themselves.
        tcgetattr(pty, &tc);
        tc.c_iflag |= (IXON|ICRNL);
        tc.c_lflag |= (ECHO|ECHOE|ECHOK|ISIG|IEXTEN|ICANON);
        tc.c_oflag |= (OPOST|OCRNL);
        tcsetattr(pty, TCSANOW, &tc);
        
        // Replace std fds with the pseudo-tty.
        dup2(pty, STDIN_FILENO);
        dup2(pty, STDOUT_FILENO);
        dup2(pty, STDERR_FILENO);

        execvp(argv[0], argv);
        FORCE(0, ERR_NO_EXEC);
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
    set_tty_raw(STDOUT_FILENO);
    set_tty_raw(pty);

    return pty;
}

void select_loop(int pty, int sock)
{
    unsigned int i;
    struct watched_fds *watcher = new_watcher(8);

    watch_fd(watcher, STDIN_FILENO);
    watch_fd(watcher, pty);
    watch_fd(watcher, sock);

    while (watch_for_data(watcher) != -1) {
        FORCE(!FD_ISSET(sock, &watcher->error_set), ERR_IO_ERROR); // No server.
        FORCE(!FD_ISSET(pty, &watcher->error_set),  ERR_IO_ERROR); // No terminal.
        
        if(FD_ISSET(pty, &watcher->read_set)) {
            transfer_mapped(write_crnl, pty, STDOUT_FILENO);

            // Sometimes reads from the tty fail when programs do strange
            // things with it, but these cases can be safely ignored.

            unflag_fd(watcher, pty);
        }

        if (FD_ISSET(sock, &watcher->read_set)) {
            int new_fd;
            while((new_fd = accept(sock, NULL, NULL)) == -1 && errno == EINTR) ;
            FORCE(new_fd != -1 || errno == ECONNABORTED, ERR_NO_ACCEPT);
            watch_fd(watcher, new_fd);
            unflag_fd(watcher, sock);
        }

        for (i = 0; i < watcher->len; i++) {
            if (FD_ISSET(watcher->fds[i], &watcher->error_set)) {
                unwatch_fd(watcher, watcher->fds[i]);
                unflag_fd(watcher, watcher->fds[i]);
            }
            
            if (FD_ISSET(watcher->fds[i], &watcher->read_set)) {
                // Incoming newlines from the Unix Domain sockets need to be
                // transformed into carriage returns for processing by the pty.
                int ret = transfer_mapped(write_cr, watcher->fds[i], pty);

                FORCE(ret != -1, ERR_IO_ERROR);
                
                if (!ret) { // We have reached end-of-file.
                    close(watcher->fds[i]);
                    unwatch_fd(watcher, watcher->fds[i]);

                    // If the only fds left are the master socket and the
                    // pseudo-terminal, then we're done.
                    if (watcher->len == 2)
                        exit(EXIT_SUCCESS);

                    i--; // Repeat the loop at this position, as a new fd
                         // is now in the place of the deleted one.
                }

                unflag_fd(watcher, watcher->fds[i]);
            }
        }
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
    FORCE(socket_fd != -1, ERR_NO_UDSERVER);
    pty_fd      = forkpty(argc - 2, argv + 2); // Skip the command and socket path.

    atexit(cleanup);
     
    select_loop(pty_fd, socket_fd);

    exit(EXIT_SUCCESS);
}
