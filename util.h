#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define FORCE(expr, message) if (!(expr)) { \
    printf("ERROR: %s\n", message);         \
    perror(NULL);                           \
    exit(1);                                \
}

#endif
