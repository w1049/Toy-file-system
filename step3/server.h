//
// Server functions
//

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

#include "common.h"

void mainloop(int port, void *(*client_init)(int),
          int (*serve)(int, char *, int, void *));

#endif