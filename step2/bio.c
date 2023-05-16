#include <string.h>
#include <stdio.h>

#include "bio.h"
#include "log.h"

#define NCYL 1024
#define NSEC 63

static char diskfile[NCYL * NSEC][256];

void binfo(int *ncyl, int *nsec) {
    *ncyl = NCYL;
    *nsec = NSEC;
}

void bread(int blockno, char *buf) {
    memcpy(buf, diskfile[blockno], 256);
}

void bwrite(int blockno, char *buf) {
    memcpy(diskfile[blockno], buf, 256);
    FILE *fp = fopen("diskfile", "w");
    fseek(fp, blockno * 256, SEEK_SET);
    fwrite(buf, 256, 1, fp);
    fclose(fp);
    char str[25565], *s = str;
    s += sprintf(s, "bwrite: blockno=%d, bufdata:\n", blockno);
    for (int i = 0; i < 256; i++) {
        s += sprintf(s, "%02x ", (unsigned char)buf[i]);
        if (i % 16 == 15) {
            s += sprintf(s, "\n");
        }
    }
    Log("%s", str);
}
