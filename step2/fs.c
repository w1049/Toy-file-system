#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include "log.h"
#include "bio.h"

// hex and dec
static inline int hex2int(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}
// static char hex[] = "0123456789abcdef";

typedef unsigned int uint;

static inline uint min(uint a, uint b) { return a < b ? a : b; }

// Block size in bytes
#define BSIZE 256

#define NDIRECT 11

#define MAXFILEB (NDIRECT + APB + APB * APB)

enum {
    T_DIR = 1,              // Directory
    T_FILE = 2,             // File
};

struct dinode { // 64 bytes
    unsigned short type: 2;          // File type: 0empty, 1dir or 2file
    unsigned short mode: 4;          // File mode: rwrw for owner and others
    unsigned short uid: 10;          // Owner id
    unsigned short nlink;            // Number of links to inode
    uint mtime;             // Last modified time
    uint size;              // Size in bytes
    uint addrs[NDIRECT+2];  // Data block addresses
};

// inode in memory
struct inode {
    uint inum;
    unsigned short type: 2;          // File type: 0empty, 1dir or 2file
    unsigned short mode: 4;          // File mode: rwrw for owner and others
    unsigned short uid: 10;          // Owner id
    unsigned short nlink;            // Number of links to inode
    uint mtime;             // Last modified time
    uint size;              // Size in bytes
    uint addrs[NDIRECT+2];  // Data block addresses
};

static inline void prtinode(struct inode *ip) {
    Log("inode %u: type %u, mode %x, uid %u, nlink %u, mtime %u, size %u",
        ip->inum, ip->type, ip->mode, ip->uid, ip->nlink, ip->mtime, ip->size);
}

#define MAXNAME 12

struct dirent { // 16 bytes
    uint inum;
    char name[MAXNAME];
};

// Super block, at most 256 bytes
struct superblock {
    uint magic;             // Magic number, "MYFS"
    uint size;              // Size in blocks
    uint nblocks;           // Number of data blocks
    uint ninodes;           // Number of inodes
    uint inodestart;        // Block number of first inode
    uint bmapstart;         // Block number of first free map block
} sb;

#define NINODES 1024

// inodes per block
#define IPB (BSIZE / sizeof(struct dinode))

// bits per block
#define BPB (BSIZE * 8)

// dirent per block
#define DPB (BSIZE / sizeof(struct dirent))

// addresses per block
#define APB (BSIZE / sizeof(uint))

#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)

int fsize;
int nblocks;
int ninodesblocks = (NINODES / IPB) + 1;
int nbitmap;
int nmeta;

// Disk layout:
// [ superblock | inode blocks | free bit map | data blocks ]

// zero a block
void bzro(uint bno) {
    char buf[BSIZE];
    memset(buf, 0, BSIZE);
    bwrite(bno, buf);
}

// allocate a block
uint balloc() {
    for (int i = 0; i < sb.size; i += BPB) {
        char buf[BSIZE];
        bread(BBLOCK(i, sb), buf);
        for (int j = 0; j < BPB; j++) {
            int m = 1 << (j % 8);
            if ((buf[j / 8] & m) == 0) {
                buf[j / 8] |= m;
                bwrite(BBLOCK(i, sb), buf);
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
    char buf[BSIZE];
    bread(BBLOCK(bno, sb), buf);
    int i = bno % BPB;
    int m = 1 << (i % 8);
    if ((buf[i / 8] & m) == 0)
        Warn("freeing free block");
    buf[i / 8] &= ~m;
    bwrite(BBLOCK(bno, sb), buf);
}

// get the inode with inum
// remember to free it!
// return NULL if not found
struct inode* iget(uint inum) {
    if (inum < 0 || inum >= sb.ninodes) {
        Warn("iget: inum out of range");
        return NULL;
    }
    char buf[BSIZE];
    bread(IBLOCK(inum, sb), buf);
    struct dinode *dip = (struct dinode*)buf + inum % IPB;
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
    memcpy(ip->addrs, dip->addrs, sizeof(ip->addrs));
    Log("iget: inum %d", inum);
    prtinode(ip);
    return ip;
}

// allocate an inode
// remember to free it!
// return NULL if no inode is available
struct inode *ialloc(short type) {
    char buf[BSIZE];
    for (int i = 0; i < sb.ninodes; i++) {
        bread(IBLOCK(i, sb), buf);
        struct dinode *dip = (struct dinode*)buf + i % IPB;
        if (dip->type == 0) {
            memset(dip, 0, sizeof(struct dinode));
            dip->type = type;
            bwrite(IBLOCK(i, sb), buf);
            struct inode *ip = calloc(1, sizeof(struct inode));
            ip->inum = i;
            ip->type = type;
            Log("ialloc: inum %d, type=%d", i, type);
            prtinode(ip);
            return ip;
        }
    }
    Error("ialloc: no inodes");
    return NULL;
}

// write the inode to disk
void iupdate(struct inode* ip) {
    char buf[BSIZE];
    bread(IBLOCK(ip->inum, sb), buf);
    struct dinode *dip = (struct dinode*)buf + ip->inum % IPB;
    dip->type = ip->type;
    dip->mode = ip->mode;
    dip->uid = ip->uid;
    dip->nlink = ip->nlink;
    dip->mtime = ip->mtime;
    dip->size = ip->size;
    memcpy(dip->addrs, ip->addrs, sizeof(ip->addrs));
    bwrite(IBLOCK(ip->inum, sb), buf);
}

// free all data blocks of an inode, but not the inode itself
void itrunc(struct inode *ip) {
    char buf[BSIZE];
    int apb = APB;

    for (int i = 0; i < NDIRECT; i++)
        if (ip->addrs[i]) {
            bfree(ip->addrs[i]);
            ip->addrs[i] = 0;
        }

    if (ip->addrs[NDIRECT]) {
        bread(ip->addrs[NDIRECT], buf);
        uint *addrs = (uint*)buf;
        for (int i = 0; i < apb; i++)
            if (addrs[i])
                bfree(addrs[i]);
        bfree(ip->addrs[NDIRECT]);
        ip->addrs[NDIRECT] = 0;
    }

    if (ip->addrs[NDIRECT+1]) {
        bread(ip->addrs[NDIRECT+1], buf);
        uint *addrs = (uint*)buf;
        char buf2[BSIZE];
        for (int i = 0; i < apb; i++) {
            if (addrs[i]) {
                bread(addrs[i], buf2);
                uint *addrs2 = (uint*)buf2;
                for (int j = 0; j < apb; j++)
                    if (addrs2[j])
                        bfree(addrs2[j]);
                bfree(addrs[i]);
            }
        }
        bfree(ip->addrs[NDIRECT+1]);
        ip->addrs[NDIRECT+1] = 0;
    }

    ip->size = 0;
    iupdate(ip);
}

// if not exists, alloc a block for ip->addrs bn
// return the block number
// will not update the inode, update it yourself when freeing
int bmap(struct inode *ip, uint bn) {
    char buf[BSIZE];
    uint addr;
    if (bn < NDIRECT) {
        addr = ip->addrs[bn];
        if (!addr) addr = ip->addrs[bn] = balloc();
        return addr;
    } else if (bn < NDIRECT + APB) {
        bn -= NDIRECT;
        uint saddr = ip->addrs[NDIRECT]; // single addr
        if (!saddr) saddr = ip->addrs[NDIRECT] = balloc();
        bread(saddr, buf);
        uint *addrs = (uint*)buf;
        addr = addrs[bn];
        if (!addr) {
            addr = addrs[bn] = balloc();
            bwrite(saddr, buf);
        }
        return addr;
    } else if (bn < MAXFILEB) {
        bn -= NDIRECT + APB;
        uint a = bn / APB, b = bn % APB;
        uint daddr = ip->addrs[NDIRECT+1]; // double addr
        if (!daddr) daddr = ip->addrs[NDIRECT+1] = balloc();
        bread(daddr, buf);
        uint *addrs = (uint*)buf;

        uint saddr = addrs[a]; // single addr
        if (!saddr) {
            saddr = addrs[a] = balloc();
            bwrite(daddr, buf);
        }
        bread(saddr, buf);
        addrs = (uint*)buf;

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

int readi(struct inode *ip, char *dst, uint off, uint n) {
    char buf[BSIZE];
    if(off > ip->size || off + n < off)
        return -1;
    if(off + n > ip->size) // read till EOF
        n = ip->size - off;

    for(uint tot = 0, m; tot < n; tot += m, off += m, dst += m){
        bread(bmap(ip, off / BSIZE), buf);
        m = min(n - tot, BSIZE - off % BSIZE);
        memcpy(dst, buf + off % BSIZE, m);
    }
    return n;
}

// may change the size
// will update
int writei(struct inode *ip, char *src, uint off, uint n) {
    char buf[BSIZE];
    if(off > ip->size || off + n < off)
        return -1; // off is larger than size || off overflow
    if(off + n > MAXFILEB * BSIZE)
        return -1; // too large

    for(uint tot = 0, m; tot < n; tot += m, off += m, src += m) {
        bread(bmap(ip, off / BSIZE), buf);
        m = min(n - tot, BSIZE - off % BSIZE);
        memcpy(buf + off % BSIZE, src, m);
        bwrite(bmap(ip, off / BSIZE), buf);
    }

    if(n > 0 && off > ip->size){
        ip->size = off;
        iupdate(ip);
    }
    return n;
}

#define UID 666
#define ROOTINO 0

// create a file in pinum
// will not check name
int icreate(short type, char *name, uint pinum) {
    struct inode *ip = ialloc(type);
    if (!ip) return 1;
    ip->mode = 0b1111;
    ip->uid = UID;
    ip->nlink = 1;
    ip->mtime = time(NULL);
    ip->size = 0;
    uint inum = ip->inum;
    if (type == T_DIR) {
        struct dirent des[2];
        des[0].inum = inum;
        strcpy(des[0].name, ".");
        des[1].inum = pinum;
        strcpy(des[1].name, "..");
        writei(ip, (char *)&des, ip->size, sizeof(des));
    } else iupdate(ip);
    Log("Create inode %d, mtime=%d", ip->inum, ip->mtime);
    prtinode(ip);
    free(ip);
    if (pinum != inum) { // root will not enter here
        ip = iget(pinum);
        struct dirent de;
        de.inum = inum;
        strcpy(de.name, name);
        writei(ip, (char *)&de, ip->size, sizeof(de));
        free(ip);
    }
    return 0;
}

// append a file to dir
// will not check if the file already exists
int dappend(struct inode *ip, struct dirent *de) {
    return writei(ip, (char *)de, ip->size, sizeof(struct dirent));
}

#define MAXARGS 5
// parse a line into commands
int parse(char *line, char *cmds[]) {
    char *p;
    int ncmd = 0;
    p = strtok(line, " ");
    while (p) {
        cmds[ncmd++] = p;
        if (ncmd >= MAXARGS) return -1;
        p = strtok(NULL, " ");
    }
    cmds[ncmd] = NULL;
    return ncmd;
}

#define MAGIC 0x5346594d
// return 0 for ok, 1 for not ok
static inline int checkformat() {
    if (sb.magic == MAGIC) return 0;
    printf("Please format first!\n");
    return 1;
}

// return a negative value to exit
int cmd_f(int argc, char *argv[]) {
    char buf[BSIZE];

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
    sb.inodestart = 1; // 0 for superblock
    sb.bmapstart = 1 + ninodesblocks;
    Log("sb: magic=0x%x size=%d nblocks=%d ninodes=%d inodestart=%d bmapstart=%d",
        sb.magic, sb.size, sb.nblocks, sb.ninodes, sb.inodestart, sb.bmapstart);    

    memset(buf, 0, BSIZE);
    memcpy(buf, &sb, sizeof(sb));
    bwrite(0, buf);

    // mark meta blocks as in use
    for (int i = 0; i < nmeta; i += BPB) {
        memset(buf, 0, BSIZE);
        for (int j = 0; j < BPB; j++)
            if (i + j < nmeta)
                buf[j / 8] |= 1 << (j % 8);
        bwrite(BBLOCK(i, sb), buf);
    }
    
    // make root dir
    icreate(T_DIR, NULL, 0);
    printf("Yes\n");
    Log("Format");
    return 0;
}

uint pwdinum = 0;

int validname(char *name) {
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
    if (!ip) return 0;

    char *buf = malloc(ip->size); // TODO fixed-size buf
    readi(ip, buf, 0, ip->size);
    struct dirent *de = (struct dirent*)buf;

    int result = NINODES;
    int nfile = ip->size / sizeof(struct dirent);
    for (int i = 0; i < nfile; i++) {
        if (de[i].inum == NINODES) continue; // deleted
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
    if (!ip) return 0;

    char *buf = malloc(ip->size); // TODO fixed-size buf
    readi(ip, buf, 0, ip->size);
    struct dirent *de = (struct dirent*)buf;

    int nfile = ip->size / sizeof(struct dirent);
    for (int i = 0; i < nfile; i++) {
        if (de[i].inum == NINODES) continue; // deleted
        if (de[i].inum == inum) {
            de[i].inum = NINODES;
            writei(ip, (char *)&de[i], i * sizeof(struct dirent), sizeof(struct dirent));
            break;
        }
    }
    free(buf);
    free(ip);
    return 0;
}

int cmd_mk(int argc, char *argv[]) {
    if (checkformat()) return 0;
    if (argc != 2) {
        printf("Usage: mk <filename>\n");
        return 0;
    }
    if (!validname(argv[1])) {
        printf("Invalid name!\n");
        return 0;
    }
    if (findinum(argv[1]) != NINODES) {
        printf("Already exists!\n");
        return 0;
    }
    icreate(T_FILE, argv[1], pwdinum);
    return 0;
}
int cmd_mkdir(int argc, char *argv[]) {
    if (checkformat()) return 0;
    if (argc != 2) {
        printf("Usage: mkdir <dirname>\n");
        return 0;
    }
    if (!validname(argv[1])) {
        printf("Invalid name!\n");
        return 0;
    }
    if (findinum(argv[1]) != NINODES) {
        printf("Already exists!\n");
        return 0;
    }
    icreate(T_DIR, argv[1], pwdinum);
    return 0;
}
int cmd_rm(int argc, char *argv[]) {
    if (checkformat()) return 0;
    if (argc != 2) {
        printf("Usage: rm <filename>\n");
        return 0;
    }
    uint inum = findinum(argv[1]);
    if (inum == NINODES) {
        printf("Not found!\n");
        return 0;
    }
    struct inode *ip = iget(inum);
    if (!ip) return 0;
    if (ip->type != T_FILE) {
        printf("Not a file, please use rmdir\n");
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
    return 0;
}
int cmd_cd(int argc, char *argv[]) {
    if (checkformat()) return 0;
    if (argc != 2) {
        printf("Usage: cd <dirname>\n");
        return 0;
    }
    uint inum = findinum(argv[1]);
    if (inum == NINODES) {
        printf("Not found!\n");
        return 0;
    }
    struct inode *ip = iget(inum);
    if (!ip) return 0;
    if (ip->type != T_DIR) {
        printf("Not a directory\n");
        free(ip);
        return 0;
    }
    pwdinum = inum;
    free(ip);
    return 0;
}
int cmd_rmdir(int argc, char *argv[]) {
    if (checkformat()) return 0;
    if (argc != 2) {
        printf("Usage: rmdir <dirname>\n");
        return 0;
    }
    uint inum = findinum(argv[1]);
    if (inum == NINODES) {
        printf("Not found!\n");
        return 0;
    }
    struct inode *ip = iget(inum);
    if (!ip) return 0;
    if (ip->type != T_DIR) {
        printf("Not a dir, please use rm\n");
        free(ip);
        return 0;
    }

    // if dir is not empty
    int empty = 1;
    char *buf = malloc(ip->size); // TODO fixed-size buf
    readi(ip, buf, 0, ip->size);
    struct dirent *de = (struct dirent*)buf;

    int nfile = ip->size / sizeof(struct dirent);
    for (int i = 0; i < nfile; i++) {
        if (de[i].inum == NINODES) continue; // deleted
        if (strcmp(de[i].name, ".") == 0 || strcmp(de[i].name, "..") == 0) continue;
        empty = 0;
        break;
    }
    free(buf);

    if (!empty) {
        printf("Directory not empty!\n");
        free(ip);
        return 0;
    }

    // ok, delete
    itrunc(ip);
    free(ip);
    delinum(inum);
    return 0;
}

// do not check if pwd is valid
// now no arg
int cmd_ls(int argc, char *argv[]) {
    if (checkformat()) return 0;
    struct inode *ip = iget(pwdinum);
    if (!ip) return 0;

    char *buf = malloc(ip->size); // TODO fixed-size buf
    readi(ip, buf, 0, ip->size);
    struct dirent *de = (struct dirent*)buf;

    int nfile = ip->size / sizeof(struct dirent);
    char str[100]; // for time
    for (int i = 0; i < nfile; i++) {
        if (de[i].inum == NINODES) continue; // deleted
        struct inode *sub = iget(de[i].inum);
        Log("Entry %d: name=%s", de[i].inum, de[i].name); prtinode(sub);
        time_t mtime = sub->mtime;
        struct tm *tmptr = localtime(&mtime);
        strftime(str, sizeof(str), "%Y-%m-%d %H:%M:%S", tmptr);
        printf("%s\t%s\t%s\t%d bytes\n", sub->type == T_DIR ? "d" : "f", de[i].name, str, sub->size);
        free(sub);
    }
    free(buf);
    free(ip);
    Log("List");
    return 0;
}
int cmd_cat(int argc, char *argv[]) {
    if (checkformat()) return 0;
    if (argc != 2) {
        printf("Usage: cat <filename>\n");
        return 0;
    }
    uint inum = findinum(argv[1]);
    if (inum == NINODES) {
        printf("Not found!\n");
        return 0;
    }
    struct inode *ip = iget(inum);
    if (!ip) return 0;
    if (ip->type != T_FILE) {
        printf("Not a file\n");
        free(ip);
        return 0;
    }
    char *buf = malloc(ip->size + 1);
    readi(ip, buf, 0, ip->size);
    buf[ip->size] = 0;
    printf("%s\n", buf);
    free(buf);
    free(ip);
    return 0;
}
int cmd_w(int argc, char *argv[]) {
    if (checkformat()) return 0;
    if (argc != 4) {
        printf("Usage: w <filename> <length> <data>\n");
        return 0;
    }
    uint inum = findinum(argv[1]);
    if (inum == NINODES) {
        printf("Not found!\n");
        return 0;
    }
    struct inode *ip = iget(inum);
    if (!ip) return 0;
    if (ip->type != T_FILE) {
        printf("Not a file\n");
        free(ip);
        return 0;
    }

    int len = atoi(argv[2]);
    // TODO convert hex
    writei(ip, argv[3], 0, len);
    ip->size = len;
    // TODO check for size
    iupdate(ip);
    free(ip);
    return 0;
}
int cmd_i(int argc, char *argv[]) {
    if (checkformat()) return 0;
    printf("Goodbye!\n");
    Log("Exit");
    return -1;
}
int cmd_d(int argc, char *argv[]) {
    if (checkformat()) return 0;
    printf("Goodbye!\n");
    Log("Exit");
    return -1;
}
int cmd_e(int argc, char *argv[]) {
    printf("Goodbye!\n");
    Log("Exit");
    return -1;
}

static struct {
    const char *name;
    int (*handler)(int, char **);
} cmd_table[] = {
    {"f", cmd_f},
    {"mk", cmd_mk},
    {"mkdir", cmd_mkdir},
    {"rm", cmd_rm},
    {"cd", cmd_cd},
    {"rmdir", cmd_rmdir},
    {"ls", cmd_ls},
    {"cat", cmd_cat},
    {"w", cmd_w},
    {"i", cmd_i},
    {"d", cmd_d},
    {"e", cmd_e},
};

void sbinit() {
    char buf[BSIZE];
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
    static char *cmds[MAXARGS];
    int NCMD = sizeof(cmd_table) / sizeof(cmd_table[0]);
    while (1) {
        fgets(buf, sizeof(buf), stdin);
        if (feof(stdin)) break;
        buf[strlen(buf) - 1] = 0;
        Log("use command: %s", buf);
        int ncmd = parse(buf, cmds);
        if (ncmd < 0) {
            printf("Too many args!\n");
            Warn("Too many args");
            continue;
        }
        if (ncmd == 0) continue;
        int ret = 1;
        for (int i = 0; i < NCMD; i++)
            if (strcmp(cmds[0], cmd_table[i].name) == 0) {
                ret = cmd_table[i].handler(ncmd, cmds);
                break;
            }
        if (ret == 1) {
            printf("No such command!\n");
            Warn("No such command");
        }
        if (ret < 0) break;
    }

    log_close();
}
