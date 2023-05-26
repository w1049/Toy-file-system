//
// Block Input / Output
//

#include "bio.h"

#include <stdio.h>
#include <string.h>

#include "log.h"

#define NCYL 1024
#define NSEC 63

static uchar diskfile[NCYL * NSEC][256];

void binfo(int *ncyl, int *nsec) {
    *ncyl = NCYL;
    *nsec = NSEC;
}

void bread(int blockno, uchar *buf) { memcpy(buf, diskfile[blockno], 256); }

void bwrite(int blockno, uchar *buf) {
    memcpy(diskfile[blockno], buf, 256);
    // char str[25565], *s = str;
    // s += sprintf(s, "bwrite: blockno=%d, bufdata:\n", blockno);
    // for (int i = 0; i < 256; i++) {
    //     s += sprintf(s, "%02x ", (uchar)buf[i]);
    //     if (i % 16 == 15) {
    //         s += sprintf(s, "\n");
    //     }
    // }
    // Log("%s", str);
}
