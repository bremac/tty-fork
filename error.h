#ifndef ERROR_H
#define ERROR_H

#define ERR_NO_MEMORY   "Failed to allocate memory."
#define ERR_NO_UDSERVER "Unable to create a server socket."
#define ERR_NO_UDCLIENT "Unable to connect to server socket."
#define ERR_NO_TTY      "The parent terminal is not a valid TTY."
#define ERR_NO_PTY      "Unable to allocate a new pseudo-terminal."
#define ERR_NO_FORK     "Unable to fork the child process."
#define ERR_NO_EXEC     "Unable to execute the specified command."
#define ERR_NO_SESSION  "Unable to execute the child in a new session."
#define ERR_NO_ACCEPT   "Unable to accept incoming connections."
#define ERR_IO_ERROR    "An IO transfer was abnormally aborted."

#endif
