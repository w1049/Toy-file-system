#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "bio.h"
#include "log.h"
#include "common.h"

typedef unsigned int uint;

static inline uint min(uint a, uint b) { return a < b ? a : b; }
static inline uint max(uint a, uint b) { return a < b ? b : a; }

// Block size in bytes
#define BSIZE 256

#define NDIRECT 10

#define MAXFILEB (NDIRECT + APB + APB * APB)

enum {
    T_DIR = 1,   // Directory
    T_FILE = 2,  // File
};

struct dinode {               // 64 bytes
    ushort type : 2;  // File type: 0empty, 1dir or 2file
    ushort mode : 4;  // File mode: rwrw for owner and others
    ushort uid : 10;  // Owner id
    ushort nlink;     // Number of links to inode
    uint mtime;               // Last modified time
    uint size;                // Size in bytes
    uint blocks;              // Number of blocks, may be larger than size
    uint addrs[NDIRECT + 2];  // Data block addresses
};

// inode in memory
struct inode {
    uint inum;
    ushort type : 2;  // File type: 0empty, 1dir or 2file
    ushort mode : 4;  // File mode: rwrw for owner and others
    ushort uid : 10;  // Owner id
    ushort nlink;     // Number of links to inode
    uint mtime;               // Last modified time
    uint size;                // Size in bytes
    uint blocks;              // Number of blocks, may be larger than size
                              // not consider index blocks
    uint addrs[NDIRECT + 2];  // Data block addresses
};

static inline void prtinode(struct inode *ip) {
    Debug(
        "inode %u: type %u, mode %x, uid %u, nlink %u, mtime %u, size %u, "
        "blocks %u",
        ip->inum, ip->type, ip->mode, ip->uid, ip->nlink, ip->mtime, ip->size,
        ip->blocks);
}

// the longest file name is 11 bytes (the last byte is '\0')
#define MAXNAME 12

struct dirent {  // 16 bytes
    uint inum;
    char name[MAXNAME];
};

// Super block, at most 256 bytes
struct superblock {
    uint magic;       // Magic number, "MYFS"
    uint size;        // Size in blocks
    uint nblocks;     // Number of data blocks
    uint ninodes;     // Number of inodes
    uint inodestart;  // Block number of first inode
    uint bmapstart;   // Block number of first free map block
} sb;

// total number of inodes
#define NINODES 1024

// inodes per block
#define IPB (BSIZE / sizeof(struct dinode))

// bits per block
#define BPB (BSIZE * 8)

// dirent per block
#define DPB (BSIZE / sizeof(struct dirent))

// addresses per block
#define APB (BSIZE / sizeof(uint))

// block containing inode i
#define IBLOCK(i) ((i) / IPB + sb.inodestart)
// block of free map containing bit for block b
#define BBLOCK(b) ((b) / BPB + sb.bmapstart)

int fsize;
int nblocks;
int ninodesblocks = (NINODES / IPB) + 1;
int nbitmap;
int nmeta;

// Disk layout:
// [ superblock | inode blocks | free bit map | data blocks ]

// zero a block
void bzro(uint bno) {
    uchar buf[BSIZE];
    memset(buf, 0, BSIZE);
    bwrite(bno, buf);
}

// allocate a block
uint balloc() {
    for (int i = 0; i < sb.size; i += BPB) {
        uchar buf[BSIZE];
        bread(BBLOCK(i), buf);
        for (int j = 0; j < BPB; j++) {
            int m = 1 << (j % 8);
            if ((buf[j / 8] & m) == 0) {
                buf[j / 8] |= m;
                bwrite(BBLOCK(i), buf);
                bzro(i + j);
                return i + j;
            }
        }
    }
    Warn("balloc: out of blocks");
    return 0;
}

// free a block
void bfree(uint bno) {
    uchar buf[BSIZE];
    bread(BBLOCK(bno), buf);
    int i = bno % BPB;
    int m = 1 << (i % 8);
    if ((buf[i / 8] & m) == 0) Warn("freeing free block");
    buf[i / 8] &= ~m;
    bwrite(BBLOCK(bno), buf);
}

// get the inode with inum
// remember to free it!
// return NULL if not found
struct inode *iget(uint inum) {
    if (inum < 0 || inum >= sb.ninodes) {
        Warn("iget: inum out of range");
        return NULL;
    }
    uchar buf[BSIZE];
    bread(IBLOCK(inum), buf);
    struct dinode *dip = (struct dinode *)buf + inum % IPB;
    if (dip->type == 0) {
        Warn("iget: no such inode");
        return NULL;
    }
    struct inode *ip = calloc(1, sizeof(struct inode));
    ip->inum = inum;
    ip->type = dip->type;
    ip->mode = dip->mode;
    ip->uid = dip->uid;
    ip->nlink = dip->nlink;
    ip->mtime = dip->mtime;
    ip->size = dip->size;
    ip->blocks = dip->blocks;
    memcpy(ip->addrs, dip->addrs, sizeof(ip->addrs));
    Debug("iget: inum %d", inum);
    prtinode(ip);
    return ip;
}

// allocate an inode
// remember to free it!
// return NULL if no inode is available
struct inode *ialloc(short type) {
    uchar buf[BSIZE];
    for (int i = 0; i < sb.ninodes; i++) {
        bread(IBLOCK(i), buf);
        struct dinode *dip = (struct dinode *)buf + i % IPB;
        if (dip->type == 0) {
            memset(dip, 0, sizeof(struct dinode));
            dip->type = type;
            bwrite(IBLOCK(i), buf);
            struct inode *ip = calloc(1, sizeof(struct inode));
            ip->inum = i;
            ip->type = type;
            Debug("ialloc: inum %d, type=%d", i, type);
            prtinode(ip);
            return ip;
        }
    }
    Error("ialloc: no inodes");
    return NULL;
}

// write the inode to disk
void iupdate(struct inode *ip) {
    uchar buf[BSIZE];
    bread(IBLOCK(ip->inum), buf);
    struct dinode *dip = (struct dinode *)buf + ip->inum % IPB;
    dip->type = ip->type;
    dip->mode = ip->mode;
    dip->uid = ip->uid;
    dip->nlink = ip->nlink;
    dip->mtime = ip->mtime;
    dip->size = ip->size;
    dip->blocks = ip->blocks;
    memcpy(dip->addrs, ip->addrs, sizeof(ip->addrs));
    bwrite(IBLOCK(ip->inum), buf);
}

// free all data blocks of an inode, but not the inode itself
void itrunc(struct inode *ip) {
    uchar buf[BSIZE];
    int apb = APB;

    for (int i = 0; i < NDIRECT; i++)
        if (ip->addrs[i]) {
            bfree(ip->addrs[i]);
            ip->addrs[i] = 0;
        }

    if (ip->addrs[NDIRECT]) {
        bread(ip->addrs[NDIRECT], buf);
        uint *addrs = (uint *)buf;
        for (int i = 0; i < apb; i++)
            if (addrs[i]) bfree(addrs[i]);
        bfree(ip->addrs[NDIRECT]);
        ip->addrs[NDIRECT] = 0;
    }

    if (ip->addrs[NDIRECT + 1]) {
        bread(ip->addrs[NDIRECT + 1], buf);
        uint *addrs = (uint *)buf;
        uchar buf2[BSIZE];
        for (int i = 0; i < apb; i++) {
            if (addrs[i]) {
                bread(addrs[i], buf2);
                uint *addrs2 = (uint *)buf2;
                for (int j = 0; j < apb; j++)
                    if (addrs2[j]) bfree(addrs2[j]);
                bfree(addrs[i]);
            }
        }
        bfree(ip->addrs[NDIRECT + 1]);
        ip->addrs[NDIRECT + 1] = 0;
    }

    ip->size = 0;
    ip->blocks = 0;
    iupdate(ip);
}

// if not exists, alloc a block for ip->addrs bn
// return the block number
// will not update the inode, update it yourself when freeing
// you'd better use this continuously
// will not increase ip->blocks, writei will do that
int bmap(struct inode *ip, uint bn) {
    uchar buf[BSIZE];
    uint addr;
    if (bn < NDIRECT) {
        addr = ip->addrs[bn];
        if (!addr) addr = ip->addrs[bn] = balloc();
        return addr;
    } else if (bn < NDIRECT + APB) {
        bn -= NDIRECT;
        uint saddr = ip->addrs[NDIRECT];  // single addr
        if (!saddr) saddr = ip->addrs[NDIRECT] = balloc();
        bread(saddr, buf);
        uint *addrs = (uint *)buf;
        addr = addrs[bn];
        if (!addr) {
            addr = addrs[bn] = balloc();
            bwrite(saddr, buf);
        }
        return addr;
    } else if (bn < MAXFILEB) {
        bn -= NDIRECT + APB;
        uint a = bn / APB, b = bn % APB;
        uint daddr = ip->addrs[NDIRECT + 1];  // double addr
        if (!daddr) daddr = ip->addrs[NDIRECT + 1] = balloc();
        bread(daddr, buf);
        uint *addrs = (uint *)buf;

        uint saddr = addrs[a];  // single addr
        if (!saddr) {
            saddr = addrs[a] = balloc();
            bwrite(daddr, buf);
        }
        bread(saddr, buf);
        addrs = (uint *)buf;

        addr = addrs[b];
        if (!addr) {
            addr = addrs[b] = balloc();
            bwrite(saddr, buf);
        }
        return addr;
    } else {
        Warn("bmap: bn too large");
        return 0;
    }
}

int readi(struct inode *ip, uchar *dst, uint off, uint n) {
    uchar buf[BSIZE];
    if (off > ip->size || off + n < off) return -1;
    if (off + n > ip->size)  // read till EOF
        n = ip->size - off;

    for (uint tot = 0, m; tot < n; tot += m, off += m, dst += m) {
        bread(bmap(ip, off / BSIZE), buf);
        m = min(n - tot, BSIZE - off % BSIZE);
        memcpy(dst, buf + off % BSIZE, m);
    }
    return n;
}

// may change the size
// will update
int writei(struct inode *ip, uchar *src, uint off, uint n) {
    uchar buf[BSIZE];
    if (off > ip->size || off + n < off)
        return -1;  // off is larger than size || off overflow
    if (off + n > MAXFILEB * BSIZE) return -1;  // too large

    for (uint tot = 0, m; tot < n; tot += m, off += m, src += m) {
        bread(bmap(ip, off / BSIZE), buf);
        m = min(n - tot, BSIZE - off % BSIZE);
        memcpy(buf + off % BSIZE, src, m);
        bwrite(bmap(ip, off / BSIZE), buf);
    }

    if (n > 0 && off > ip->size) {  // size is larger
        ip->size = off;
        ip->blocks =
            max(1 + (off - 1) / BSIZE, ip->blocks);  // blocks may change
        iupdate(ip);
    }
    return n;
}

#define MAGIC 0x5346594d

#define PrtYes()         \
    do {                 \
        printf("Yes\n"); \
        Log("Success");  \
    } while (0)
#define PrtNo(x)        \
    do {                \
        printf("No\n"); \
        Error(x);       \
    } while (0)
#define CheckIP(x)               \
    do {                         \
        if (!ip) {               \
            printf("No\n");      \
            Error("ip is NULL"); \
            return x;            \
        }                        \
    } while (0)
#define CheckFmt()                  \
    do {                            \
        if (sb.magic != MAGIC) {    \
            PrtNo("Not formatted"); \
            return 0;               \
        }                           \
    } while (0)
#define Parse(maxargs)       \
    char *argv[maxargs + 1]; \
    int argc = parse(args, argv, maxargs);
// for (int i = 0; i < argc; i++) Debug("argv[%d] = %s", i, argv[i]);
// if (maxargs != MAXARGS) Debug("argv[argc] = %s", argv[argc]);

#define UID 666
#define ROOTINO 0

// create a file in parent pinum
// will not check name
// return 0 for success
int icreate(short type, char *name, uint pinum) {
    struct inode *ip = ialloc(type);
    CheckIP(1);
    ip->mode = 0b1111;
    ip->uid = UID;
    ip->nlink = 1;
    ip->mtime = time(NULL);
    ip->size = 0;
    ip->blocks = 0;
    uint inum = ip->inum;
    if (type == T_DIR) {
        struct dirent des[2];
        des[0].inum = inum;
        strcpy(des[0].name, ".");
        des[1].inum = pinum;
        strcpy(des[1].name, "..");
        writei(ip, (uchar *)&des, ip->size, sizeof(des));
    } else
        iupdate(ip);
    Log("Create %s inode %d, inside directory inode %d",
        type == T_DIR ? "dir" : "file", ip->inum, pinum);
    prtinode(ip);
    free(ip);
    if (pinum != inum) {  // root will not enter here
                          // for normal files, add it to the parent directory
        ip = iget(pinum);
        struct dirent de;
        de.inum = inum;
        strcpy(de.name, name);
        writei(ip, (uchar *)&de, ip->size, sizeof(de));
        free(ip);
    }
    return 0;
}

#define MAXARGS 6
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
int cmd_f(char *args) {
    uchar buf[BSIZE];

    // calculate args and write superblock
    int ncyl, nsec;
    binfo(&ncyl, &nsec);
    fsize = ncyl * nsec;
    Log("ncyl=%d nsec=%d fsize=%d", ncyl, nsec, fsize);
    nbitmap = (fsize / BPB) + 1;
    nmeta = 1 + ninodesblocks + nbitmap;
    nblocks = fsize - nmeta;
    Log("ninodeblocks=%d nbitmap=%d nblocks=%d", fsize, nmeta, nblocks);

    sb.magic = MAGIC;
    sb.size = fsize;
    sb.nblocks = nblocks;
    sb.ninodes = NINODES;
    sb.inodestart = 1;  // 0 for superblock
    sb.bmapstart = 1 + ninodesblocks;
    Log("sb: magic=0x%x size=%d nblocks=%d ninodes=%d inodestart=%d "
        "bmapstart=%d",
        sb.magic, sb.size, sb.nblocks, sb.ninodes, sb.inodestart, sb.bmapstart);

    memset(buf, 0, BSIZE);
    memcpy(buf, &sb, sizeof(sb));
    bwrite(0, buf);

    // mark meta blocks as in use
    for (int i = 0; i < nmeta; i += BPB) {
        memset(buf, 0, BSIZE);
        for (int j = 0; j < BPB; j++)
            if (i + j < nmeta) buf[j / 8] |= 1 << (j % 8);
        bwrite(BBLOCK(i), buf);
    }

    // make root dir
    if (!icreate(T_DIR, NULL, 0)) PrtYes();
    return 0;
}

uint pwdinum = 0;

int is_name_valid(char *name) {
    int len = strlen(name);
    if (len >= MAXNAME) return 0;
    if (name[0] == '.') return 0;
    if (strcmp(name, "/") == 0) return 0;
    // static char invalid[] = "\\/<>?\":| @#$&();*";
    // int invalidlen = strlen(invalid);
    // for (int i = 0; i < len; i++)
    //     for (int j = 0; j < invalidlen; j++)
    //         if (name[i] == invalid[j]) return 0;
    return 1;
}

// NINODES for not found
// return inum of the file
uint findinum(char *name) {
    struct inode *ip = iget(pwdinum);
    CheckIP(NINODES);

    uchar *buf = malloc(ip->size);
    readi(ip, buf, 0, ip->size);
    struct dirent *de = (struct dirent *)buf;

    int result = NINODES;
    int nfile = ip->size / sizeof(struct dirent);
    for (int i = 0; i < nfile; i++) {
        if (de[i].inum == NINODES) continue;  // deleted
        if (strcmp(de[i].name, name) == 0) {
            result = de[i].inum;
            break;
        }
    }
    free(buf);
    free(ip);
    return result;
}

// delete inode from pwd
int delinum(uint inum) {
    struct inode *ip = iget(pwdinum);
    CheckIP(0);

    uchar *buf = malloc(ip->size);
    readi(ip, buf, 0, ip->size);
    struct dirent *de = (struct dirent *)buf;

    int nfile = ip->size / sizeof(struct dirent);
    for (int i = 0; i < nfile; i++) {
        if (de[i].inum == NINODES) continue;  // deleted
        if (de[i].inum == inum) {
            de[i].inum = NINODES;
            writei(ip, (uchar *)&de[i], i * sizeof(struct dirent),
                   sizeof(struct dirent));
            break;
        }
    }
    free(buf);
    free(ip);
    return 0;
}

int cmd_mk(char *args) {
    CheckFmt();
    Parse(MAXARGS);
    if (argc < 1) {
        PrtNo("Usage: mk <filename>");
        return 0;
    }
    if (!is_name_valid(argv[0])) {
        PrtNo("Invalid name!");
        return 0;
    }
    if (findinum(argv[0]) != NINODES) {
        PrtNo("Already exists!");
        return 0;
    }
    if (!icreate(T_FILE, argv[0], pwdinum)) PrtYes();
    return 0;
}
int cmd_mkdir(char *args) {
    CheckFmt();
    Parse(MAXARGS);
    if (argc < 1) {
        PrtNo("Usage: mkdir <dirname>");
        return 0;
    }
    if (!is_name_valid(argv[0])) {
        PrtNo("Invalid name!");
        return 0;
    }
    if (findinum(argv[0]) != NINODES) {
        PrtNo("Already exists!");
        return 0;
    }
    if (!icreate(T_DIR, argv[0], pwdinum)) PrtYes();
    return 0;
}
int cmd_rm(char *args) {
    CheckFmt();
    Parse(MAXARGS);
    if (argc < 1) {
        PrtNo("Usage: rm <filename>");
        return 0;
    }
    uint inum = findinum(argv[0]);
    if (inum == NINODES) {
        PrtNo("Not found!");
        return 0;
    }
    struct inode *ip = iget(inum);
    CheckIP(0);
    if (ip->type != T_FILE) {
        PrtNo("Not a file, please use rmdir");
        free(ip);
        return 0;
    }
    if (--ip->nlink == 0) {
        itrunc(ip);
    } else {
        iupdate(ip);
    }
    free(ip);

    delinum(inum);
    PrtYes();
    return 0;
}
int cmd_cd(char *args) {
    CheckFmt();
    Parse(MAXARGS);
    if (argc < 1) {
        PrtNo("Usage: cd <dirname>");
        return 0;
    }
    uint inum = findinum(argv[0]);
    if (inum == NINODES) {
        PrtNo("Not found!");
        return 0;
    }
    struct inode *ip = iget(inum);
    CheckIP(0);
    if (ip->type != T_DIR) {
        PrtNo("Not a directory");
        free(ip);
        return 0;
    }
    pwdinum = inum;
    free(ip);
    PrtYes();
    return 0;
}
int cmd_rmdir(char *args) {
    CheckFmt();
    Parse(MAXARGS);
    if (argc < 1) {
        PrtNo("Usage: rmdir <dirname>");
        return 0;
    }
    uint inum = findinum(argv[0]);
    if (inum == NINODES) {
        PrtNo("Not found!");
        return 0;
    }
    struct inode *ip = iget(inum);
    CheckIP(0);
    if (ip->type != T_DIR) {
        PrtNo("Not a dir, please use rm");
        free(ip);
        return 0;
    }

    // if dir is not empty
    int empty = 1;
    uchar *buf = malloc(ip->size);
    readi(ip, buf, 0, ip->size);
    struct dirent *de = (struct dirent *)buf;

    int nfile = ip->size / sizeof(struct dirent);
    for (int i = 0; i < nfile; i++) {
        if (de[i].inum == NINODES) continue;  // deleted
        if (strcmp(de[i].name, ".") == 0 || strcmp(de[i].name, "..") == 0)
            continue;
        empty = 0;
        break;
    }
    free(buf);

    if (!empty) {
        PrtNo("Directory not empty!");
        free(ip);
        return 0;
    }

    // ok, delete
    itrunc(ip);
    free(ip);
    delinum(inum);
    PrtYes();
    return 0;
}

// for ls
struct entry {
    short type;
    char name[MAXNAME];
    uint mtime;
    uint size;
};

int cmp_ls(const void *a, const void *b) {
    struct entry *da = (struct entry *)a;
    struct entry *db = (struct entry *)b;
    if (da->type != db->type) {
        if (da->type == T_DIR)
            return -1;  // dir first
        else
            return 1;
    }
    // same type, compare name
    return strcmp(da->name, db->name);
}

// do not check if pwd is valid
// now no arg
int cmd_ls(char *args) {
    CheckFmt();
    struct inode *ip = iget(pwdinum);
    CheckIP(0);

    uchar *buf = malloc(ip->size);
    readi(ip, buf, 0, ip->size);
    struct dirent *de = (struct dirent *)buf;

    int nfile = ip->size / sizeof(struct dirent), n = 0;
    struct entry *entries = malloc(nfile * sizeof(struct entry));
    for (int i = 0; i < nfile; i++) {
        if (de[i].inum == NINODES) continue;  // deleted
        if (strcmp(de[i].name, ".") == 0 || strcmp(de[i].name, "..") == 0)
            continue;
        struct inode *sub = iget(de[i].inum);
        entries[n].type = sub->type;
        strcpy(entries[n].name, de[i].name);
        entries[n].mtime = sub->mtime;
        entries[n++].size = sub->size;
        free(sub);
    }
    qsort(entries, n, sizeof(struct entry), cmp_ls);
    char str[100];  // for time
    printf("\33[1mType\tUpdate time\tSize\tName\033[0m\n");
    for (int i = 0; i < n; i++) {
        time_t mtime = entries[i].mtime;
        struct tm *tmptr = localtime(&mtime);
        strftime(str, sizeof(str), "%m-%d %H:%M", tmptr);
        int d = entries[i].type == T_DIR;
        printf("%s\t", d ? "d" : "f");
        printf("%s\t%d\t", str, entries[i].size);
        printf(d ? "\033[34m\33[1m%s\033[0m\n" : "%s\n", entries[i].name);
        Log("%s\t%s\t%s\t%dB", d ? "d" : "f", entries[i].name, str,
            entries[i].size);
    }
    free(entries);
    free(buf);
    free(ip);

    return 0;
}
int cmd_cat(char *args) {
    CheckFmt();
    Parse(MAXARGS);
    if (argc < 1) {
        PrtNo("Usage: cat <filename>");
        return 0;
    }
    uint inum = findinum(argv[0]);
    if (inum == NINODES) {
        PrtNo("Not found!");
        return 0;
    }
    struct inode *ip = iget(inum);
    CheckIP(0);
    if (ip->type != T_FILE) {
        PrtNo("Not a file");
        free(ip);
        return 0;
    }

    uchar *buf = malloc(ip->size + 1);
    readi(ip, buf, 0, ip->size);
    buf[ip->size] = 0;
    printf("%s\n", buf);

    free(buf);
    free(ip);
    return 0;
}
int cmd_w(char *args) {
    CheckFmt();
    Parse(2);
    if (argc < 2) {
        PrtNo("Usage: w <filename> <length> <data>");
        return 0;
    }
    uint inum = findinum(argv[0]);
    if (inum == NINODES) {
        PrtNo("Not found!");
        return 0;
    }
    struct inode *ip = iget(inum);
    CheckIP(0);
    if (ip->type != T_FILE) {
        PrtNo("Not a file");
        free(ip);
        return 0;
    }

    uint len = atoi(argv[1]);
    char *data = argv[2];

    writei(ip, (uchar *)data, 0, len);

    if (len < ip->size) {
        // if the new data is shorter, truncate
        ip->size = len;
        iupdate(ip);
    }

    free(ip);
    PrtYes();
    return 0;
}
int cmd_i(char *args) {
    CheckFmt();
    Parse(3) if (argc < 3) {
        PrtNo("Usage: i <filename> <pos> <length> <data>");
        return 0;
    }
    uint inum = findinum(argv[0]);
    if (inum == NINODES) {
        PrtNo("Not found!");
        return 0;
    }
    struct inode *ip = iget(inum);
    CheckIP(0);
    if (ip->type != T_FILE) {
        PrtNo("Not a file");
        free(ip);
        return 0;
    }
    uint pos = atoi(argv[1]);
    uint len = atoi(argv[2]);
    char *data = argv[3];

    if (pos >= ip->size) {
        pos = ip->size;
        writei(ip, (uchar *)data, pos, len);
    } else {
        uchar *buf = malloc(ip->size - pos);
        // [pos, size) -> [pos+len, size+len)
        readi(ip, buf, pos, ip->size - pos);
        writei(ip, (uchar *)data, pos, len);
        writei(ip, buf, pos + len, ip->size - pos);
    }

    free(ip);
    PrtYes();
    return 0;
}
int cmd_d(char *args) {
    CheckFmt();
    Parse(MAXARGS);
    if (argc < 3) {
        PrtNo("Usage: d <filename> <pos> <length>");
        return 0;
    }
    uint inum = findinum(argv[0]);
    if (inum == NINODES) {
        PrtNo("Not found!");
        return 0;
    }
    struct inode *ip = iget(inum);
    CheckIP(0);
    if (ip->type != T_FILE) {
        PrtNo("Not a file");
        free(ip);
        return 0;
    }
    uint pos = atoi(argv[1]);
    uint len = atoi(argv[2]);

    if (pos + len >= ip->size) {
        ip->size = pos;
        iupdate(ip);
    } else {
        // [pos + len, size) -> [pos, size - len)
        uint copylen = ip->size - pos - len;
        uchar *buf = malloc(copylen);
        readi(ip, buf, pos + len, copylen);
        writei(ip, buf, pos, copylen);
        ip->size -= len;
        iupdate(ip);
    }

    free(ip);
    PrtYes();
    return 0;
}
int cmd_e(char *args) {
    printf("Goodbye!\n");
    Log("Exit");
    return -1;
}

static struct {
    const char *name;
    int (*handler)(char *);
} cmd_table[] = {
    {"f", cmd_f},   {"mk", cmd_mk},   {"mkdir", cmd_mkdir},
    {"rm", cmd_rm}, {"cd", cmd_cd},   {"rmdir", cmd_rmdir},
    {"ls", cmd_ls}, {"cat", cmd_cat}, {"w", cmd_w},
    {"i", cmd_i},   {"d", cmd_d},     {"e", cmd_e},
};

void sbinit() {
    uchar buf[BSIZE];
    bread(0, buf);
    memcpy(&sb, buf, sizeof(sb));
}

int main(int argc, char *argv[]) {
    log_init("fs.log");

    assert(BSIZE % sizeof(struct dinode) == 0);
    assert(BSIZE % sizeof(struct dirent) == 0);

    sbinit();

    // command
    static char buf[1024];
    int NCMD = sizeof(cmd_table) / sizeof(cmd_table[0]);
    while (1) {
        fgets(buf, sizeof(buf), stdin);
        if (feof(stdin)) break;
        buf[strlen(buf) - 1] = 0;
        Log("use command: %s", buf);
        char *p = strtok(buf, " ");
        int ret = 1;
        for (int i = 0; i < NCMD; i++)
            if (strcmp(p, cmd_table[i].name) == 0) {
                ret = cmd_table[i].handler(p + strlen(p) + 1);
                break;
            }
        if (ret == 1) {
            PrtNo("No such command");
        }
        if (ret < 0) break;
    }

    log_close();
}
