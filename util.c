#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "util.h"

#define READ_BUFFER_LEN 2048

// Transfer data to the tty, transforming \n to \n\r.
ssize_t write_crnl(int to_fd, char *buffer, ssize_t len)
{
    ssize_t ret;
    char *end;
    ssize_t count, offset = 0;
    
    // Translate \n to \n\r.
    do {
        count = len - offset;
        end   = memchr(buffer + offset, '\n', count);

        // XXX: Check for off-by-one.
        if (end) count = end - buffer - offset + 1;

        // Write the data out up until the next newline.
        while((ret = write(to_fd, buffer + offset, count)) == -1 &&
              errno == EINTR) ;

        // Write out any required carraige returns to the FD.
        while(end && (ret = write(to_fd, "\r", 1)) == -1 &&
              errno == EINTR) ;

        offset += count;
    } while(offset < len && ret != -1);
    
    return ret;
}

ssize_t write_cr(int to_fd, char *buffer, ssize_t len)
{
    ssize_t ret;
    ssize_t count;
    char *newline = buffer;

    // Transform all of the newlines to carriage returns.
    while ((newline = memchr(newline, '\n', count)) != NULL) {
        *newline = '\r';
        count = len - (newline - buffer);
    }
    
    while((ret = write(to_fd, buffer, len)) == -1 &&
          errno == EINTR) ;

    return ret;
}

ssize_t transfer_mapped(tty_writer do_write, int from_fd, int to_fd)
{
    ssize_t len;
    char read_buffer[READ_BUFFER_LEN];

    while((len = read(from_fd, read_buffer, READ_BUFFER_LEN)) == -1 &&
          errno == EINTR) ;

    if (len < 0) return len;
 
    return do_write(to_fd, read_buffer, len);
}
