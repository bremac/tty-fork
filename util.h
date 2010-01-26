#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define FORCE(expr, message) if (!(expr)) { \
    printf("ERROR: %s\n", message);         \
    if(errno != 0) perror(NULL);            \
    exit(1);                                \
}

typedef ssize_t(*tty_writer)(int, char *, ssize_t);

ssize_t write_crnl(int to_fd, char *buffer, ssize_t len);
ssize_t write_cr(int to_fd, char *buffer, ssize_t len);
ssize_t transfer_mapped(tty_writer do_write, int from_fd, int to_fd);

#endif
