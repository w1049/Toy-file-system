#ifndef __BIO_H__
#define __BIO_H__

#include "common.h"

void binfo(int *ncyl, int *nsec);
void bread(int blockno, uchar *buf);
void bwrite(int blockno, uchar *buf);

#endif
