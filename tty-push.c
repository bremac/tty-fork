#include <stdlib.h>
#include <stdio.h>
#include "util.h"

const char *USAGE = "Usage: tty-push <path>";

int main(int argc, char **argv)
{
    int socket;

    if (argc != 2) {
        puts(USAGE);
        exit(1);
    }

    socket = make_domain_client(argv[1]);

    while(transfer_mapped(safe_write, STDIN_FILENO, socket) > 0) ;   

    return EXIT_SUCCESS;
}
