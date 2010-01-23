#ifndef WATCH_H
#define WATCH_H

#include <sys/select.h>

struct watched_fds {
    fd_set       read_set, error_set;
    int         *fds, highest;
    unsigned int len, max;
};

struct watched_fds *new_watcher();
void free_watcher(struct watched_fds *watcher);
void watch_fd(struct watched_fds *watcher, int fd);
int unwatch_fd(struct watched_fds *watcher, int fd);
int watch_for_data(struct watched_fds *watcher);

#endif
