#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define FORCE(expr, message)                             \
if (!(expr)) {                                           \
    write_crnl(STDERR_FILENO, "ERROR: ", 7);             \
    write_crnl(STDERR_FILENO, message, strlen(message)); \
    write_crnl(STDERR_FILENO, "\n", 1);                  \
    if(errno != 0) perror(NULL);                         \
    exit(1);                                             \
}

typedef ssize_t(*tty_writer)(int, char *, ssize_t);

ssize_t write_crnl(int to_fd, char *buffer, ssize_t len);
ssize_t write_cr(int to_fd, char *buffer, ssize_t len);
ssize_t transfer_mapped(tty_writer do_write, int from_fd, int to_fd);

#endif
