#ifndef WATCH_H
#define WATCH_H

#include <sys/select.h>

struct watched_fds {
    fd_set       read_set, error_set;
    int         *fds, highest;
    unsigned int len, max; // The number of watched FDs, and space allocated.
};

/* Allocate a watched_fds structure, preallocating space for len   *
 * watched elements in the file descriptor array.                  */
struct watched_fds *new_watcher(unsigned len);

/* Deallocate the space associated with a watched_fds structure.   */
void free_watcher(struct watched_fds *watcher);

/* Begin watching a FD, allocating new space if necessary. The     *
 * associated spaces in the fd_sets are zeroed before returning.   */
void watch_fd(struct watched_fds *watcher, int fd);

/* Stop watching a FD, clearing all relevant fd_sets. This doesn't *
 * deallocate any new space allocated in the watched_fds struct.   */
void unwatch_fd(struct watched_fds *watcher, int fd);

/* Perform a blocking poll event until at least one of the watched *
 * file descriptors receives data or experiences an error. When    *
 * this function returns, the associated flags may be read from    *
 * the read_set and error_set fields of the fd_set.                *
 * This function returns the number of events (both read and write *
 * may be flagged for the same fd), or -1 if an error occurred.    */
int watch_for_data(struct watched_fds *watcher);

#endif
