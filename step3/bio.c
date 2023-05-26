#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "bio.h"
#include "log.h"
#include "client.h"
MSGDEF;

// hex and dec
static inline int hex2int(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}
static char hex[] = "0123456789abcdef";

int fd;

#define BSIZE 256

void bioinit(int serverfd) {
    fd = serverfd;
}

static int ncyl, nsec;
void binfo(int *pncyl, int *pnsec) {
    send(fd, "I\n", 3, 0);
    int n = recv(fd, msg, 4096, 0);
    if (n < 0) err(1, ERROR "recv()");
    msg[n] = 0;
    sscanf(msg, "%d %d", pncyl, pnsec);
    ncyl = *pncyl, nsec = *pnsec;
}

void bread(int blockno, uchar *buf) {
    msginit();
    msgprintf("R %d %d\n", blockno / nsec, blockno % nsec);
    msgsend(fd);
    int n = recv(fd, msg, 4096, 0);
    if (n < 0) err(1, ERROR "recv()");
    char *data = &msg[4]; // "Yes xxxxx"
    for (int i = 0; i < BSIZE; i++) {
        int a = hex2int(data[i * 2]);
        int b = hex2int(data[i * 2 + 1]);
        if (a < 0 || b < 0) { return; }
        buf[i] = a * 16 + b;
    }
}

void bwrite(int blockno, uchar *buf) {
    static char hexbuf[BSIZE * 2 + 1];
    uchar *p = buf;
    for (int i = 0; i < BSIZE; i++) {
        hexbuf[i * 2] = hex[p[i] / 16];
        hexbuf[i * 2 + 1] = hex[p[i] % 16];
    }
    hexbuf[BSIZE * 2] = '\0';
    msginit();
    msgprintf("W %d %d %s\n", blockno / nsec, blockno % nsec, hexbuf);
    // printf("send %s\n", msg);
    msgsend(fd);
    int n = recv(fd, msg, 4096, 0); // recv "Yes" or "No"
    if (n < 0) err(1, ERROR "recv()");
    // msg[n] = 0;
    // printf("Write recv: %s", msg);
}
