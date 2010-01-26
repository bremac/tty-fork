.PHONY: clean

CC=gcc
FLAGS=-Wall

TTY_FORK_BIN=tty-fork
TTY_PUSH_BIN=tty-push

UTILS=util.c util.h
TTY_FORK_DEPS=tty-fork.c watch.c watch.h $(UTILS)
TTY_PUSH_DEPS=tty-push.c $(UTILS)

all: $(TTY_FORK_BIN) $(TTY_PUSH_BIN)
	strip -s $(TTY_FORK_BIN) $(TTY_PUSH_BIN)

clean:
	rm -f $(TTY_FORK_BIN) $(TTY_PUSH_BIN)

$(TTY_FORK_BIN): $(TTY_FORK_DEPS)
	$(CC) $(FLAGS) $(TTY_FORK_DEPS) -o $(TTY_FORK_BIN)

$(TTY_PUSH_BIN): $(TTY_PUSH_DEPS)
	$(CC) $(FLAGS) $(TTY_PUSH_DEPS) -o $(TTY_PUSH_BIN)