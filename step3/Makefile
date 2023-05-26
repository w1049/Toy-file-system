SRC = $(wildcard *.c)
CC = gcc
CFLAGS += -Wall -Werror -fsanitize=address -g -DDEBUG

all: fs disk client

fs: fs.o bio.o server.o client.o
	$(CC) $(CFLAGS) $^ -o $@

disk: disk.o server.o
	$(CC) $(CFLAGS) $^ -o $@

client: client.o clientmain.o
	$(CC) $(CFLAGS) $^ -o $@

include ${SRC:.c=.d}
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.d: %.c
	@set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

clean:
	rm -f *.o *.d fs disk client

.PHONY: all clean
