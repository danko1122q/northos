#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#define stat xv6_stat // Prevents conflict between host OS 'stat' and xv6 'stat'
#include "../include/fs.h"
#include "../include/param.h"
#include "../include/stat.h"
#include "../include/types.h"

// Basic compile-time check to ensure logic holds
#ifndef static_assert
#define static_assert(a, b)                                                    \
	do {                                                                   \
		switch (0)                                                     \
		case 0:                                                        \
		case (a):;                                                     \
	} while (0)
#endif

#define NINODES 200 // Total capacity of the inode table

/**
 * Disk Layout Visualization:
 * [ boot block | superblock | log | inode blocks | free bit map | data blocks ]
 */

int nbitmap =
	FSSIZE / (BSIZE * 8) + 1;     // Blocks needed for the free-block bitmap
int ninodeblocks = NINODES / IPB + 1; // Blocks needed to store 200 inodes
int nlog = LOGSIZE; // Number of blocks for the transaction log
int nmeta;	    // Sum of all metadata blocks
int nblocks;	    // Remaining blocks available for raw data

int fsfd;	      // File descriptor for the resulting disk image (fs.img)
struct superblock sb; // The "header" of the file system
char zeroes[BSIZE];   // Used to initialize the disk with empty space
uint freeinode = 1;   // Pointer to the next available inode index
uint freeblock;	      // Pointer to the next available data block index

// Function prototypes for low-level block/inode manipulation
void balloc(int);
void wsect(uint, void *);
void winode(uint, struct dinode *);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);

// --- Byte Order Utilities ---
// These ensure the disk image is Little-Endian, even if created on a Big-Endian
// host.
ushort xshort(ushort x) {
	ushort y;
	uchar *a = (uchar *)&y;
	a[0] = x;
	a[1] = x >> 8;
	return y;
}

uint xint(uint x) {
	uint y;
	uchar *a = (uchar *)&y;
	a[0] = x;
	a[1] = x >> 8;
	a[2] = x >> 16;
	a[3] = x >> 24;
	return y;
}

int main(int argc, char *argv[]) {
	int i, cc, fd;
	uint rootino, inum, off;
	struct dirent de;
	char buf[BSIZE];
	struct dinode din;

	static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

	if (argc < 2) {
		fprintf(stderr, "Usage: mkfs fs.img files...\n");
		exit(1);
	}

	// Sanity checks: Ensure structures fit perfectly into disk blocks
	assert((BSIZE % sizeof(struct dinode)) == 0);
	assert((BSIZE % sizeof(struct dirent)) == 0);

	// Create/Open the disk image file
	fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (fsfd < 0) {
		perror(argv[1]);
		exit(1);
	}

	// Calculate layout geometry
	nmeta = 2 + nlog + ninodeblocks + nbitmap;
	nblocks = FSSIZE - nmeta;

	// Initialize Superblock fields
	sb.size = xint(FSSIZE);
	sb.nblocks = xint(nblocks);
	sb.ninodes = xint(NINODES);
	sb.nlog = xint(nlog);
	sb.logstart = xint(2);
	sb.inodestart = xint(2 + nlog);
	sb.bmapstart = xint(2 + nlog + ninodeblocks);

	printf("nmeta %d (boot, super, log blocks %u inode blocks %u, bitmap "
	       "blocks %u) blocks %d total %d\n",
	       nmeta, nlog, ninodeblocks, nbitmap, nblocks, FSSIZE);

	freeblock = nmeta; // Data blocks start immediately after metadata

	// Wipe the entire image with zeros
	for (i = 0; i < FSSIZE; i++)
		wsect(i, zeroes);

	// Write the Superblock into sector 1 (sector 0 is the boot block)
	memset(buf, 0, sizeof(buf));
	memmove(buf, &sb, sizeof(sb));
	wsect(1, buf);

	// Create the Root Directory inode
	rootino = ialloc(T_DIR);
	assert(rootino == ROOTINO);

	// Add "." (current directory) entry to root
	bzero(&de, sizeof(de));
	de.inum = xshort(rootino);
	strcpy(de.name, ".");
	iappend(rootino, &de, sizeof(de));

	// Add ".." (parent directory) entry to root
	bzero(&de, sizeof(de));
	de.inum = xshort(rootino);
	strcpy(de.name, "..");
	iappend(rootino, &de, sizeof(de));

	// --- Inject Files ---
	// Loop through files passed via command line (like rm, ls, cat)
	for (i = 2; i < argc; i++) {
		char *filepath = argv[i];
		char *filename = basename(strdup(filepath));

		// Remove leading underscore (used by build system to
		// distinguish binaries)
		if (filename[0] == '_')
			++filename;

		if ((fd = open(filepath, 0)) < 0) {
			perror(filepath);
			exit(1);
		}

		// Allocate a new inode for the file
		inum = ialloc(T_FILE);

		// Add the file entry to the root directory
		bzero(&de, sizeof(de));
		de.inum = xshort(inum);
		strncpy(de.name, filename, DIRSIZ);
		iappend(rootino, &de, sizeof(de));

		// Read content from host file and append to the virtual disk
		// file
		while ((cc = read(fd, buf, sizeof(buf))) > 0)
			iappend(inum, buf, cc);

		close(fd);
	}

	// Standardize the root directory size to block boundaries
	rinode(rootino, &din);
	off = xint(din.size);
	off = ((off / BSIZE) + 1) * BSIZE;
	din.size = xint(off);
	winode(rootino, &din);

	// Mark all used blocks in the bitmap
	balloc(freeblock);

	exit(0);
}

// Write a block/sector to the disk image
void wsect(uint sec, void *buf) {
	if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE) {
		perror("lseek");
		exit(1);
	}
	if (write(fsfd, buf, BSIZE) != BSIZE) {
		perror("write");
		exit(1);
	}
}

// Write an inode to the inode table on disk
void winode(uint inum, struct dinode *ip) {
	char buf[BSIZE];
	uint bn;
	struct dinode *dip;

	bn = IBLOCK(inum, sb); // Find which block contains this inode
	rsect(bn, buf);
	dip = ((struct dinode *)buf) + (inum % IPB);
	*dip = *ip;
	wsect(bn, buf);
}

// Read an inode from the inode table on disk
void rinode(uint inum, struct dinode *ip) {
	char buf[BSIZE];
	uint bn;
	struct dinode *dip;

	bn = IBLOCK(inum, sb);
	rsect(bn, buf);
	dip = ((struct dinode *)buf) + (inum % IPB);
	*ip = *dip;
}

// Read a sector from the disk image
void rsect(uint sec, void *buf) {
	if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE) {
		perror("lseek");
		exit(1);
	}
	if (read(fsfd, buf, BSIZE) != BSIZE) {
		perror("read");
		exit(1);
	}
}

// Initialize and allocate a new inode
uint ialloc(ushort type) {
	uint inum = freeinode++;
	struct dinode din;

	bzero(&din, sizeof(din));
	din.type = xshort(type);
	din.nlink = xshort(1);
	din.size = xint(0);
	winode(inum, &din);
	return inum;
}

// Update the free-block bitmap block
void balloc(int used) {
	uchar buf[BSIZE];
	int i;

	printf("balloc: first %d blocks have been allocated\n", used);
	assert(used < BSIZE * 8);
	bzero(buf, BSIZE);
	for (i = 0; i < used; i++) {
		buf[i / 8] = buf[i / 8] |
			     (0x1 << (i % 8)); // Set bits for used blocks
	}
	wsect(sb.bmapstart, buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

// Append data to an existing inode (handles direct and indirect blocks)
void iappend(uint inum, void *xp, int n) {
	char *p = (char *)xp;
	uint fbn, off, n1;
	struct dinode din;
	char buf[BSIZE];
	uint indirect[NINDIRECT];
	uint x;

	rinode(inum, &din);
	off = xint(din.size);
	while (n > 0) {
		fbn = off / BSIZE; // File block number
		assert(fbn < MAXFILE);

		// Case 1: Direct Blocks (NDIRECT = 12)
		if (fbn < NDIRECT) {
			if (xint(din.addrs[fbn]) == 0) {
				din.addrs[fbn] = xint(freeblock++);
			}
			x = xint(din.addrs[fbn]);
		}
		// Case 2: Indirect Block (Pointer to a block of pointers)
		else {
			if (xint(din.addrs[NDIRECT]) == 0) {
				din.addrs[NDIRECT] = xint(freeblock++);
			}
			rsect(xint(din.addrs[NDIRECT]), (char *)indirect);
			if (indirect[fbn - NDIRECT] == 0) {
				indirect[fbn - NDIRECT] = xint(freeblock++);
				wsect(xint(din.addrs[NDIRECT]),
				      (char *)indirect);
			}
			x = xint(indirect[fbn - NDIRECT]);
		}

		// Copy logic
		n1 = min(n, (fbn + 1) * BSIZE - off);
		rsect(x, buf);
		bcopy(p, buf + off - (fbn * BSIZE), n1);
		wsect(x, buf);
		n -= n1;
		off += n1;
		p += n1;
	}
	din.size = xint(off); // Update file size
	winode(inum, &din);
}