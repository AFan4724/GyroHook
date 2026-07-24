CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Werror
LDFLAGS ?= -ldl

all: gyroctl

gyroctl: gyroctl.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f gyroctl

.PHONY: all clean
