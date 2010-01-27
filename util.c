#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "util.h"

const unsigned int SOCKET_BACKLOG = 32;

int make_domain_server(const char *path)
{
    int sock, retval;
    struct sockaddr_un addr;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    FORCE(sock != -1, "Unable to create socket for IPC.");

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path));
    retval = bind(sock, (struct sockaddr *)&addr, strlen(path) +
                                                  sizeof(addr.sun_family));
    FORCE(!retval, "Unable to bind the IPC socket to the filesystem.");

    FORCE(!listen(sock, SOCKET_BACKLOG), "Unable to listen on socket.");

    return sock;
}

int make_domain_client(const char *path)
{
    int sock, retval;
    struct sockaddr_un addr;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    FORCE(sock != -1, "Unable to create socket for IPC.");

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path));
    retval = connect(sock, (struct sockaddr *)&addr, strlen(path) +
                                                     sizeof(addr.sun_family));
    FORCE(!retval, "Unable to connect to the IPC socket on the filesystem.");

    return sock;
}

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

// Macro to automate performing a safe write, deciding whether to return
// based on the return value, and finally increasing the count.
#define WRITE_OR_FAIL(f, b, l) {  \
        r = safe_write(f, b, l);  \
        if (r == -1) return r;    \
        else         retval += r; \
}

        WRITE_OR_FAIL(to_fd, buffer + offset, count);

        if (newline)
            WRITE_OR_FAIL(to_fd, "\r", 1);
#undef WRITE_OR_FAIL
    }
    
    return retval;
}

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

// Use a preprocessor constant to keep gcc from complaining, though
// ideally I would rather this were an unsigned integer constant.
#define READ_BUFFER_LEN 64

ssize_t transfer_mapped(tty_writer do_write, int from_fd, int to_fd)
{
    ssize_t len;
    char read_buffer[READ_BUFFER_LEN];

    while((len = read(from_fd, read_buffer, READ_BUFFER_LEN)) == -1 &&
          errno == EINTR) ;

    if (len < 0)
        return len;
 
    return do_write(to_fd, read_buffer, len);
}
