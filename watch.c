#include <stdlib.h>
#include <errno.h>
#include <sys/select.h>
#include "watch.h"
#include "util.h"

struct watched_fds *new_watcher(unsigned len)
{
    struct watched_fds *watcher = malloc(sizeof(struct watched_fds));

    FORCE(watcher != NULL, "Unable to allocate memory.");
    
    FD_ZERO(&watcher->read_set);
    FD_ZERO(&watcher->error_set);

    watcher->max = len;
    watcher->len = 0;
    watcher->highest = -1;
    watcher->fds = malloc(sizeof(int) * watcher->max);

    FORCE(watcher->fds != NULL, "Unable to allocate memory.");

    return watcher;
}

void free_watcher(struct watched_fds *watcher)
{
    free(watcher->fds);
    free(watcher);
}

void watch_fd(struct watched_fds *watcher, int fd)
{
    // Allocate more space if we need it.
    if(watcher->len >= watcher->max) {
        watcher->max *= 2;
        assert(watcher->max > watcher->len);

        watcher->fds = realloc(watcher->fds, sizeof(int) * watcher->max);
        FORCE(watcher->fds != NULL, "Unable to allocate memory.");
    }

    watcher->fds[watcher->len] = fd;
    watcher->len++;
    
    // Make sure the fd is not flagged.
    FD_CLR(fd, &watcher->read_set);
    FD_CLR(fd, &watcher->error_set);

    if(fd > watcher->highest)
        watcher->highest = fd;
}

void unwatch_fd(struct watched_fds *watcher, int fd)
{
    unsigned int i;

    for (i = 0; i < watcher->len; i++) {
        if (watcher->fds[i] == fd) {
            // Replace the removed element with the last in the array.
            watcher->fds[i] = watcher->fds[watcher->len];
            watcher->len--;

            // Make sure the fd is no longer flagged.
            FD_CLR(fd, &watcher->read_set);
            FD_CLR(fd, &watcher->error_set);

            if (watcher->highest == fd) {
                // Find the new highest file descriptor.
                watcher->highest = -1;
                for (i = 0; i < watcher->len; i++)
                    if (watcher->fds[i] > watcher->highest)
                        watcher->highest = watcher->fds[i];
            }

            return;
        }
    }
}

int watch_for_data(struct watched_fds *watcher)
{
    unsigned int i;
    int retval;
    
    FD_ZERO(&watcher->read_set);
    FD_ZERO(&watcher->error_set);

    for(i = 0; i < watcher->len; i++) {
        FD_SET(watcher->fds[i], &watcher->read_set);
        FD_SET(watcher->fds[i], &watcher->error_set);
    }

    do {
        retval = select(watcher->highest + 1, &watcher->read_set, NULL,
                        &watcher->error_set, NULL);
    } while (retval == -1 && errno == EINTR);

    return retval;
}
