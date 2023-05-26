#ifndef __SERVER_H__
#define __SERVER_H__
#include <arpa/inet.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MSGDEF static char msg[4096], *msgtmp
#define msginit() msgtmp = msg
#define msgprintf(...)                            \
    do {                                          \
        msgtmp += sprintf(msgtmp, ##__VA_ARGS__); \
    } while (0)

void mainloop(int port, void *(*client_init)(int),
          int (*serve)(int, char *, int, void *));

#endif