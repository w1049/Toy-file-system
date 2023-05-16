SRC = $(wildcard *.c)
CC = gcc
CFLAGS += -Wall -Werror

all: disk

disk: disk.c log.h
	$(CC) $(CFLAGS) disk.c -o $@

clean:
	rm -f disk

.PHONY: all clean
