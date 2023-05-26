#ifndef __COMMAN_H__
#define __COMMAN_H__

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

#define MSGSIZE 4096
#define MSGDEF static char msg[MSGSIZE], *msgtmp
#define msginit() msgtmp = msg
#define msgprintf(...)                            \
    do {                                          \
        msgtmp += sprintf(msgtmp, ##__VA_ARGS__); \
    } while (0)
#define msgsend(fd) send(fd, msg, msgtmp - msg, 0);

#define ERROR "\033[31m[Error]\033[0m "

#endif