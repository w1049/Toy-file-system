#ifndef __BIO_H__
#define __BIO_H__

void binfo(int *ncyl, int *nsec);
void bread(int blockno, char *buf);
void bwrite(int blockno, char *buf);

#endif
