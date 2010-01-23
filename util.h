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

size_t transfer(int from_fd, int to_fd);

#endif
