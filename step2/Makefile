SRC = $(wildcard *.c)
CC = gcc
CFLAGS += -Wall -Werror -fsanitize=address -g -DDEBUG

all: fs

fs: fs.o bio.o
	$(CC) $(CFLAGS) fs.o bio.o -o $@

fs.o: fs.c log.h bio.h
	$(CC) $(CFLAGS) -c fs.c -o $@

bio.o: bio.c
	$(CC) $(CFLAGS) -c bio.c -o $@

clean:
	rm -f fs fs.o bio.o

.PHONY: all clean
