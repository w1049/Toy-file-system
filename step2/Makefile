SRC = $(wildcard *.c)
CC = gcc
CFLAGS += -Wall -Werror -fsanitize=address -g -DDEBUG

all: fs

fs: fs.o bio.o log.o
	$(CC) $(CFLAGS) fs.o bio.o log.o -o $@

fs.o: fs.c log.h bio.h
	$(CC) $(CFLAGS) -c fs.c -o $@

bio.o: bio.c
	$(CC) $(CFLAGS) -c bio.c -o $@

log.o: log.c
	$(CC) $(CFLAGS) -c log.c -o $@

clean:
	rm -f fs fs.o bio.o log.o

.PHONY: all clean
