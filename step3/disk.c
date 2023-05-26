#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "common.h"
#include "log.h"
#include "server.h"
MSGDEF;

// hex and dec
static inline int hex2int(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}
static char hex[] = "0123456789abcdef";

// Block size in bytes
#define BLOCKSIZE 256
int ncyl, nsec, ttd;
uchar *diskfile;
int cur_cyl;

#define PrtYes()            \
    do {                    \
        msgprintf("Yes\n"); \
        Log("Success");     \
    } while (0)
#define PrtNo(x)           \
    do {                   \
        msgprintf("No\n"); \
        Warn(x);           \
    } while (0)
#define Parse(maxargs)       \
    char *argv[maxargs + 1]; \
    int argc = parse(args, argv, maxargs);

#define MAXARGS 5
// parse a line into args
// the most number of args is lim
// so argc <= lim
// args exceeding lim will be put into argv[argc]
// for example, parse "a b c d e f", 3
// result: argc=3, argv=["a", "b", "c", "d e f"]
int parse(char *line, char *argv[], int lim) {
    char *p;
    int argc = 0;
    p = strtok(line, " ");
    while (p) {
        argv[argc++] = p;
        if (argc >= lim) break;
        p = strtok(NULL, " ");
    }
    if (argc >= lim) {
        argv[argc] = p + strlen(p) + 1;
    } else {
        argv[argc] = NULL;
    }
    return argc;
}

// return a negative value to exit
int cmd_i(char *args) {
    msgprintf("%d %d\n", ncyl, nsec);
    Log("%d Cylinders, %d Sectors per cylinder", ncyl, nsec);
    return 0;
}
int cmd_r(char *args) {
    Parse(MAXARGS);
    static uchar buf[BLOCKSIZE * 2 + 1];
    if (argc < 2) {
        PrtNo("Invalid arguments");
        return 0;
    }
    int cyl = atoi(argv[0]);
    int sec = atoi(argv[1]);
    if (cyl >= ncyl || sec >= nsec || cyl < 0 || sec < 0) {
        PrtNo("Invalid cylinder or sector");
        return 0;
    }
    uchar *p = diskfile + (cyl * nsec + sec) * BLOCKSIZE;
    for (int i = 0; i < BLOCKSIZE; i++) {
        buf[i * 2] = hex[p[i] / 16];
        buf[i * 2 + 1] = hex[p[i] % 16];
    }
    buf[BLOCKSIZE * 2] = '\0';
    int tsleep = abs(cur_cyl - cyl) * ttd;
    usleep(tsleep * 1000);
    cur_cyl = cyl;
    msgprintf("Yes %s\n", buf);
    Log("Delay %d ms, Read successfully", tsleep);
    return 0;
}
int cmd_w(char *args) {
    Parse(MAXARGS);
    static uchar buf[BLOCKSIZE];
    if (argc < 3) {
        PrtNo("Invalid arguments");
        return 0;
    }
    int cyl = atoi(argv[0]);
    int sec = atoi(argv[1]);
    char *data = argv[2];
    if (cyl >= ncyl || sec >= nsec || cyl < 0 || sec < 0) {
        PrtNo("Invalid cylinder or sector");
        return 0;
    }
    if (strlen(data) != BLOCKSIZE * 2) {
        PrtNo("Invalid data length");
        return 0;
    }
    for (int i = 0; i < BLOCKSIZE; i++) {
        int a = hex2int(data[i * 2]);
        int b = hex2int(data[i * 2 + 1]);
        if (a < 0 || b < 0) {
            PrtNo("Invalid data");
            return 0;
        }
        buf[i] = a * 16 + b;
    }
    int tsleep = abs(cur_cyl - cyl) * ttd;
    usleep(tsleep * 1000);
    cur_cyl = cyl;
    uchar *p = diskfile + (cyl * nsec + sec) * BLOCKSIZE;
    memcpy(p, buf, BLOCKSIZE);
    msgprintf("Yes\n");
    Log("Delay %d ms, Write successfully", tsleep);
    return 0;
}
int cmd_e(char *args) {
    msgprintf("Goodbye!\n");
    Log("Exit");
    return -1;
}

static struct {
    const char *name;
    int (*handler)(char *);
} cmd_table[] = {
    {"I", cmd_i},
    {"R", cmd_r},
    {"W", cmd_w},
    {"E", cmd_e},
};

void *client_init(int connfd) { return NULL; }

int NCMD;
int serve(int fd, char *buf, int len, void *) {
    Log("use command: %s", buf);
    char *p = strtok(buf, " \r\n");
    if (!p) return 0;
    int ret = 1;
    msginit();
    for (int i = 0; i < NCMD; i++)
        if (strcmp(p, cmd_table[i].name) == 0) {
            ret = cmd_table[i].handler(p + strlen(p) + 1);
            break;
        }
    if (ret == 1) {
        PrtNo("No such command");
    }
    msgsend(fd);
    return ret;
}

int main(int argc, char *argv[]) {
    if (argc != 6)
        errx(1,
             "Usage: %s <cylinders> <sector per cylinder> "
             "<track-to-track delay> <disk-storage filename> <port>",
             argv[0]);
    // args
    ncyl = atoi(argv[1]);
    nsec = atoi(argv[2]);
    ttd = atoi(argv[3]);  // ms
    char *diskfname = argv[4];

    // open file
    log_init("disk.log");
    int fd = open(diskfname, O_RDWR | O_CREAT, 0777);
    if (fd < 0) err(1, ERROR "open %s", diskfname);

    // stretch the file
    size_t filesize = ncyl * nsec * BLOCKSIZE;
    int ret = lseek(fd, filesize - 1, SEEK_SET);
    if (ret < 0) close(fd), err(1, ERROR "lseek");

    ret = write(fd, "", 1);
    if (ret < 0) close(fd), err(1, ERROR "write last byte");

    // mmap
    diskfile = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (diskfile == MAP_FAILED) close(fd), err(1, ERROR "mmap");

    // command
    NCMD = sizeof(cmd_table) / sizeof(cmd_table[0]);
    mainloop(atoi(argv[5]), client_init, serve);

    ret = munmap(diskfile, filesize);
    if (ret < 0) close(fd), err(1, ERROR "munmap");
    close(fd);
    log_close();
}
