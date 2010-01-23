#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define READ_BUFFER_LEN 2048

size_t transfer(int from_fd, int to_fd)
{
    size_t len, ret;
    char read_buffer[READ_BUFFER_LEN];

    while((len = read(from_fd, read_buffer, READ_BUFFER_LEN)) == -1 &&
          errno == EINTR) ;
    if (len < 0) return len;
    
    while((ret = write(to_fd, read_buffer, ret)) == -1 &&
          errno == EINTR) ;
    return ret;
}
