SRC = $(wildcard *.c)
CC = gcc
CFLAGS += -Wall -Werror -fsanitize=address -g -DDEBUG

all: fs

fs: fs.o bio.o
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
	rm -f *.o *.d fs

.PHONY: all clean
