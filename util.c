#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "util.h"

ssize_t write_crnl(int to_fd, char *buffer, ssize_t len)
{
    char *newline;
    ssize_t r, retval = 0;
    ssize_t count, offset = 0;
    
    for(offset = 0; offset < len; offset += count) {
        // Set the number of characters to write to the number
        // of characters remaining in the buffer.
        count   = len - offset;
        newline = memchr(buffer + offset, '\n', count);

        // Ensure that we write no further than any newline characters.
        if (newline) {
            // Use how far the newline lies beyond the current offset
            // in the buffer, plus one to include the nl.
            count = newline - buffer - offset + 1;
        }

#define WRITE_OR_FAIL(f, b, l) {  \
        r = safe_write(f, b, l);  \
        if (r == -1) return r;    \
        else         retval += r; \
}

        WRITE_OR_FAIL(to_fd, buffer + offset, count);

        if (newline) WRITE_OR_FAIL(to_fd, "\r", 1);
#undef WRITE_OR_FAIL
    }
    
    return retval;
}

// Transfer data while transforming \n to \r.
ssize_t write_cr(int to_fd, char *buffer, ssize_t len)
{
    ssize_t count;
    char *newline = buffer;

    // Transform all of the newlines to carriage returns.
    while ((newline = memchr(newline, '\n', count)) != NULL) {
        *newline = '\r';
        count = len - (newline - buffer);
    }
    
    return safe_write(to_fd, buffer, len);
}

// Use a preprocessor constant to keep gcc from complaining.
#define READ_BUFFER_LEN 2048

ssize_t transfer_mapped(tty_writer do_write, int from_fd, int to_fd)
{
    ssize_t len;
    char read_buffer[READ_BUFFER_LEN];

    while((len = read(from_fd, read_buffer, READ_BUFFER_LEN)) == -1 &&
          errno == EINTR) ;

    if (len < 0) return len;
 
    return do_write(to_fd, read_buffer, len);
}
