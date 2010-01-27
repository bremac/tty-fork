#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

/* Create a Unix Domain socket bound to the given path. Note that    *
 * this can cause the program to exit with an error if it fails.     */
int make_domain_server(const char *path);

/* Connect to a Unix Domain socket bound to the given path. Note     *
 * that this can cause the program to exit with an error on failure. */
int make_domain_client(const char *path);

/* Perform a unistd-style write operation, covering up interruptions *
 * due to signals that the program handles.                          */
static inline ssize_t safe_write(int fd, char *buffer, ssize_t len)
{
    ssize_t ret;
    
    // Ensure we don't fail due to a signal by looping until we either
    // succeed, or fail due to a different error.
    while((ret = write(fd, buffer, len)) == -1 && errno == EINTR) ;

    return ret;
}

/* Unistd-style write, appending a carriage return to all newlines. *
 * This is necessary to handle the tty writes in raw mode, as       *
 * otherwise the cursor's column will no be reset.                  */
ssize_t write_crnl(int to_fd, char *buffer, ssize_t len);

/* Although the slave pty may be handling translation of newlines   *
 * to carriage returns, or vice versa, all input received through   *
 * the Unix Domain socket also needs to have newlines transformed   *
 * to returns, after which the PTY can handle them as it pleases.   */
ssize_t write_cr(int to_fd, char *buffer, ssize_t len);

typedef ssize_t(*tty_writer)(int, char *, ssize_t);

/* Read some data in from from_fd and write the result to to_fd     *
 * using the unistd-style writing function do_write, which may      *
 * transform the input data.                                        */
ssize_t transfer_mapped(tty_writer do_write, int from_fd, int to_fd);

/* Evaluate a condition expr. If this condition is 0, print the     *
 * string "ERROR: " followed by the necessary message, and exit.    */
#define FORCE(expr, message)                             \
if (!(expr)) {                                           \
    write_crnl(STDERR_FILENO, "ERROR: ", 7);             \
    write_crnl(STDERR_FILENO, message, strlen(message)); \
    write_crnl(STDERR_FILENO, "\n", 1);                  \
    exit(1);                                             \
}

#endif
