#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

typedef ssize_t(*tty_writer)(int, char *, ssize_t);

static inline ssize_t safe_write(int fd, char *buffer, ssize_t len)
{
    ssize_t ret;
    
    // Write the data out. Ensure we don't fail due to a signal.
    while((ret = write(fd, buffer, len)) == -1 && errno == EINTR) ;

    return ret;
}

ssize_t write_crnl(int to_fd, char *buffer, ssize_t len);
ssize_t write_cr(int to_fd, char *buffer, ssize_t len);
ssize_t transfer_mapped(tty_writer do_write, int from_fd, int to_fd);

#define FORCE(expr, message)                             \
if (!(expr)) {                                           \
    write_crnl(STDERR_FILENO, "ERROR: ", 7);             \
    write_crnl(STDERR_FILENO, message, strlen(message)); \
    write_crnl(STDERR_FILENO, "\n", 1);                  \
    exit(1);                                             \
}

#endif
