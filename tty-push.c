#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "util.h"

int make_domain_client(const char *path)
{
    int sock, retval;
    struct sockaddr_un addr;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    FORCE(sock != -1, "Unable to create socket for IPC.");

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path));
    retval = connect(sock, (struct sockaddr *)&addr, strlen(path) + sizeof(addr.sun_family));
    FORCE(!retval, "Unable to connect to the IPC socket on the filesystem.");

    return sock;
}

const char *USAGE = "Usage: tty-push <path>";

int main(int argc, char **argv)
{
    int socket;

    if (argc < 2) {
        puts(USAGE);
        exit(1);
    }

    socket = make_domain_client(argv[1]);

    while(!transfer(STDIN_FILENO, socket)) ;   

    return EXIT_SUCCESS;
}
