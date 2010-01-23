.PHONY: clean

UTILS=util.c util.h
TTY_FORK_DEPS=tty-fork.c watch.c watch.h $(UTILS)
TTY_PUSH_DEPS=tty-push.c $(UTILS)

all: tty-fork tty-push

clean:
	rm tty-fork tty-push

tty-fork: $(TTY_FORK_DEPS)
	gcc $(TTY_FORK_DEPS) -o tty-fork

tty-push: $(TTY_PUSH_DEPS)
	gcc $(TTY_PUSH_DEPS) -o tty-push