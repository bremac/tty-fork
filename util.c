#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define READ_BUFFER_LEN 2048

ssize_t transfer(int from_fd, int to_fd, int print)
{
    ssize_t len, ret;
    char read_buffer[READ_BUFFER_LEN];

    while((len = read(from_fd, read_buffer, READ_BUFFER_LEN)) == -1 &&
          errno == EINTR) ;
    if (len < 0) return len;

    if (print) { // Print the data to STDOUT.
        while((ret = write(STDOUT_FILENO, read_buffer, len)) == -1 &&
              errno == EINTR) ;
    }
    while((ret = write(to_fd, read_buffer, len)) == -1 &&
          errno == EINTR) ;
    return ret;
}
