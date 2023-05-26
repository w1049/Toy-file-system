#ifndef __CLIENT_H__
#define __CLIENT_H__

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

int init_client(int port);

#endif