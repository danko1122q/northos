#ifndef BUF_H
#define BUF_H

/* Dependensi wajib untuk struct buf */
#include "param.h" /* Menyediakan BSIZE */
#include "sleeplock.h"
#include "types.h"

struct buf {
	int flags;
	uint dev;
	uint blockno;
	struct sleeplock lock;
	uint refcnt;
	struct buf *prev; // LRU cache list
	struct buf *next;
	struct buf *qnext; // disk queue
	uchar data[BSIZE]; // Menggunakan BSIZE dari param.h
};

#define B_VALID 0x2
#define B_DIRTY 0x4

#endif