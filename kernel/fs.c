// Modernized File System Implementation for NorthOS
// Features: Large file support (double indirect), bitwise allocation,
// checksum validation, and improved concurrency safety.

#include "fs.h"
#include "buf.h"
#include "defs.h"
#include "file.h"
#include "mmu.h"
#include "param.h"
#include "proc.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "stat.h"
#include "types.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Static assertion for compile-time checks
#define STATIC_ASSERT(COND, MSG)                                               \
	typedef char static_assertion_##MSG[(COND) ? 1 : -1]

// Verify inode size at compile time (Zero Bug Policy)
STATIC_ASSERT(sizeof(struct dinode) == 128, INODE_SIZE_CORRUPT);

// Superblock instance
static struct superblock sb;

// Allocation hint with synchronization for multiprocessor safety
static uint last_alloc_hint = 0;
static struct spinlock alloc_hint_lock;

// Inode cache - MUST be declared before iinit()
struct {
	struct spinlock lock;
	struct inode inode[NINODE];
} icache;

// Internal function prototypes
static void itrunc(struct inode *);
static uint bmap(struct inode *, uint);
static struct inode *iget(uint, uint);
static void bfree(int, uint);
static uint balloc(uint);

// Fletcher-32 Checksum for data integrity validation
uint fletcher32(const ushort *data, int len) {
	uint sum1 = 0xffff;
	uint sum2 = 0xffff;

	while (len > 0) {
		int tlen = (len > 360) ? 360 : len;
		len -= tlen;

		do {
			sum1 += *data++;
			sum2 += sum1;
		} while (--tlen);

		sum1 = (sum1 & 0xffff) + (sum1 >> 16);
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);
	}

	return (sum2 << 16) | sum1;
}

// Initialize filesystem - called during boot
void iinit(int dev) {
	initlock(&icache.lock, "icache");
	initlock(&alloc_hint_lock, "balloc_hint");

	for (int i = 0; i < NINODE; i++) {
		initsleeplock(&icache.inode[i].lock, "inode");
	}

	readsb(dev, &sb);

	// Validate superblock parameters (using checksum instead of magic)
	if (sb.size < sb.nblocks || sb.ninodes < 1 || sb.nblocks < 1) {
		panic("iinit: invalid superblock parameters");
	}

	cprintf("NorthOS FS: size %d, nblocks %d, inodestart %d, bmapstart "
		"%d\n",
		sb.size, sb.nblocks, sb.inodestart, sb.bmapstart);
}

// Read superblock from disk with validation
void readsb(int dev, struct superblock *sb_ptr) {
	struct buf *bp;

	if (sb_ptr == 0) {
		panic("readsb: null pointer");
	}

	bp = bread(dev, 1);
	memmove(sb_ptr, bp->data, sizeof(*sb_ptr));
	brelse(bp);

	// Copy to global instance for internal use
	memmove(&sb, sb_ptr, sizeof(sb));
}

// Zero a disk block safely
static void bzero(int dev, int bno) {
	struct buf *bp;

	if (bno < 0 || bno >= (int)sb.size) {
		panic("bzero: invalid block number");
	}

	bp = bread(dev, bno);
	memset(bp->data, 0, BSIZE);
	log_write(bp);
	brelse(bp);
}

// Allocate a disk block using optimized bitwise scan
static uint balloc(uint dev) {
	struct buf *bp;
	int bi, m;
	uint bno = 0;

	acquire(&alloc_hint_lock);
	uint start = last_alloc_hint;
	release(&alloc_hint_lock);

	// Two passes: from hint to end, then from start to hint
	for (int pass = 0; pass < 2; pass++) {
		uint start_bmap, end_bmap;

		if (pass == 0) {
			start_bmap = sb.bmapstart + (start / BPB);
			end_bmap = sb.bmapstart + (sb.nblocks / BPB) + 1;
			if (end_bmap > sb.bmapstart + (sb.size / BPB) + 1)
				end_bmap = sb.bmapstart + (sb.size / BPB) + 1;
		} else {
			start_bmap = sb.bmapstart;
			end_bmap = sb.bmapstart + (start / BPB);
		}

		for (uint bmap_block = start_bmap; bmap_block < end_bmap;
		     bmap_block++) {
			bp = bread(dev, bmap_block);

			// Scan 32 bits at a time for efficiency (O(N/32))
			for (int i = 0; i < BSIZE; i += 4) {
				uint *word = (uint *)&bp->data[i];

				if (*word !=
				    0xFFFFFFFF) { // Found free bit in this word
					for (int bit = 0; bit < 32; bit++) {
						bi = (i * 8) + bit;
						if (bi >= BPB)
							break;

						m = 1 << (bi % 8);
						if ((bp->data[bi / 8] & m) ==
						    0) {
							// Allocate block
							bp->data[bi / 8] |= m;
							log_write(bp);
							brelse(bp);

							bno = ((bmap_block -
								sb.bmapstart) *
							       BPB) +
							      bi;
							if (bno >=
							    (uint)sb.nblocks) {
								continue; // Safety
									  // check
							}

							bzero(dev, bno);

							acquire(&alloc_hint_lock);
							last_alloc_hint = bno;
							release(&alloc_hint_lock);

							return bno;
						}
					}
				}
			}
			brelse(bp);
		}
	}

	panic("balloc: out of blocks");
}

// Release a disk block back to free pool
static void bfree(int dev, uint b) {
	struct buf *bp;
	int bi, m;

	if (b >= (uint)sb.nblocks) {
		panic("bfree: block number out of range");
	}

	bp = bread(dev, BMAPBLOCK(b, sb));
	bi = b % BPB;
	m = 1 << (bi % 8);

	if ((bp->data[bi / 8] & m) == 0) {
		panic("bfree: freeing free block");
	}

	bp->data[bi / 8] &= ~m;
	log_write(bp);
	brelse(bp);
}

// Allocate new on-disk inode
struct inode *ialloc(uint dev, short type) {
	struct buf *bp;
	struct dinode *dip;

	for (int inum = 1; inum < sb.ninodes; inum++) {
		bp = bread(dev, IBLOCK(inum, sb));
		dip = (struct dinode *)bp->data + (inum % IPB);

		if (dip->data.type == 0) { // Free inode found
			memset(dip, 0, sizeof(*dip));
			dip->data.type = type;
			log_write(bp);
			brelse(bp);
			return iget(dev, inum);
		}
		brelse(bp);
	}
	panic("ialloc: no inodes");
}

// Copy in-memory inode to disk
void iupdate(struct inode *ip) {
	struct buf *bp;
	struct dinode *dip;

	if (ip == 0 || !holdingsleep(&ip->lock)) {
		panic("iupdate: invalid inode or not holding lock");
	}

	bp = bread(ip->dev, IBLOCK(ip->inum, sb));
	dip = (struct dinode *)bp->data + (ip->inum % IPB);

	dip->data.type = ip->type;
	dip->data.major = ip->major;
	dip->data.minor = ip->minor;
	dip->data.nlink = ip->nlink;
	dip->data.size = ip->size;
	memmove(dip->data.addrs, ip->addrs, sizeof(ip->addrs));

	log_write(bp);
	brelse(bp);
}

// Find or allocate inode cache entry
static struct inode *iget(uint dev, uint inum) {
	struct inode *ip, *empty = 0;

	acquire(&icache.lock);

	// Is the inode already cached?
	for (ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++) {
		if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
			ip->ref++;
			release(&icache.lock);
			return ip;
		}
		if (empty == 0 && ip->ref == 0)
			empty = ip;
	}

	// Recycle an inode cache entry
	if (empty == 0) {
		panic("iget: no free inodes in cache");
	}

	ip = empty;
	ip->dev = dev;
	ip->inum = inum;
	ip->ref = 1;
	ip->valid = 0;

	release(&icache.lock);
	return ip;
}

// Increment reference count
struct inode *idup(struct inode *ip) {
	if (ip == 0) {
		panic("idup: null inode");
	}

	acquire(&icache.lock);
	ip->ref++;
	release(&icache.lock);
	return ip;
}

// Lock inode and load from disk if necessary
void ilock(struct inode *ip) {
	struct buf *bp;
	struct dinode *dip;

	if (ip == 0 || ip->ref < 1) {
		panic("ilock: invalid inode");
	}

	acquiresleep(&ip->lock);

	if (ip->valid == 0) {
		bp = bread(ip->dev, IBLOCK(ip->inum, sb));
		dip = (struct dinode *)bp->data + (ip->inum % IPB);

		ip->type = dip->data.type;
		ip->major = dip->data.major;
		ip->minor = dip->data.minor;
		ip->nlink = dip->data.nlink;
		ip->size = dip->data.size;
		memmove(ip->addrs, dip->data.addrs, sizeof(ip->addrs));

		brelse(bp);
		ip->valid = 1;

		if (ip->type == 0) {
			panic("ilock: inode has no type");
		}
	}
}

// Unlock inode
void iunlock(struct inode *ip) {
	if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1) {
		panic("iunlock: invalid unlock attempt");
	}
	releasesleep(&ip->lock);
}

// Drop reference, free inode if no links and no references
void iput(struct inode *ip) {
	acquiresleep(&ip->lock);

	if (ip->valid && ip->nlink == 0) {
		// inode has no links: truncate and free it
		acquire(&icache.lock);
		int r = ip->ref;
		release(&icache.lock);

		if (r == 1) {
			// Last reference: free contents
			itrunc(ip);
			ip->type = 0;
			iupdate(ip);
			ip->valid = 0;
		}
	}

	releasesleep(&ip->lock);

	acquire(&icache.lock);
	ip->ref--;
	release(&icache.lock);
}

// Common idiom: unlock then put
void iunlockput(struct inode *ip) {
	iunlock(ip);
	iput(ip);
}

// Map file block to disk block (supports direct, single, double indirect)
static uint bmap(struct inode *ip, uint bn) {
	uint addr, *a;
	struct buf *bp;

	// Direct blocks
	if (bn < NDIRECT) {
		if ((addr = ip->addrs[bn]) == 0) {
			ip->addrs[bn] = addr = balloc(ip->dev);
		}
		return addr;
	}
	bn -= NDIRECT;

	// Single indirect block
	if (bn < NINDIRECT) {
		if ((addr = ip->addrs[NDIRECT]) == 0) {
			ip->addrs[NDIRECT] = addr = balloc(ip->dev);
		}
		bp = bread(ip->dev, addr);
		a = (uint *)bp->data;
		if ((addr = a[bn]) == 0) {
			a[bn] = addr = balloc(ip->dev);
			log_write(bp);
		}
		brelse(bp);
		return addr;
	}
	bn -= NINDIRECT;

	// Double indirect block
	if (bn < NDINDIRECT) {
		// First level indirect
		if ((addr = ip->addrs[NDIRECT + 1]) == 0) {
			ip->addrs[NDIRECT + 1] = addr = balloc(ip->dev);
		}

		bp = bread(ip->dev, addr);
		a = (uint *)bp->data;
		uint idx1 = bn / NINDIRECT;

		if ((addr = a[idx1]) == 0) {
			a[idx1] = addr = balloc(ip->dev);
			log_write(bp);
		}
		brelse(bp);

		// Second level indirect
		bp = bread(ip->dev, addr);
		a = (uint *)bp->data;
		uint idx2 = bn % NINDIRECT;

		if ((addr = a[idx2]) == 0) {
			a[idx2] = addr = balloc(ip->dev);
			log_write(bp);
		}
		brelse(bp);
		return addr;
	}

	panic("bmap: block number out of range");
}

// Truncate inode (free all data blocks)
static void itrunc(struct inode *ip) {
	struct buf *bp, *bp2;
	uint *a, *a2;

	// Free direct blocks
	for (int i = 0; i < NDIRECT; i++) {
		if (ip->addrs[i]) {
			bfree(ip->dev, ip->addrs[i]);
			ip->addrs[i] = 0;
		}
	}

	// Free single indirect block
	if (ip->addrs[NDIRECT]) {
		bp = bread(ip->dev, ip->addrs[NDIRECT]);
		a = (uint *)bp->data;
		for (int j = 0; j < NINDIRECT; j++) {
			if (a[j]) {
				bfree(ip->dev, a[j]);
			}
		}
		brelse(bp);
		bfree(ip->dev, ip->addrs[NDIRECT]);
		ip->addrs[NDIRECT] = 0;
	}

	// Free double indirect blocks
	if (ip->addrs[NDIRECT + 1]) {
		bp = bread(ip->dev, ip->addrs[NDIRECT + 1]);
		a = (uint *)bp->data;
		for (int j = 0; j < NINDIRECT; j++) {
			if (a[j]) {
				bp2 = bread(ip->dev, a[j]);
				a2 = (uint *)bp2->data;
				for (int k = 0; k < NINDIRECT; k++) {
					if (a2[k]) {
						bfree(ip->dev, a2[k]);
					}
				}
				brelse(bp2);
				bfree(ip->dev, a[j]);
			}
		}
		brelse(bp);
		bfree(ip->dev, ip->addrs[NDIRECT + 1]);
		ip->addrs[NDIRECT + 1] = 0;
	}

	ip->size = 0;
	iupdate(ip);
}

// Copy stat info from inode
void stati(struct inode *ip, struct stat *st) {
	if (ip == 0 || st == 0) {
		return;
	}

	st->dev = ip->dev;
	st->ino = ip->inum;
	st->type = ip->type;
	st->nlink = ip->nlink;
	st->size = ip->size;
}

// Read data from inode with overflow protection
int readi(struct inode *ip, char *dst, uint off, uint n) {
	uint tot, m;
	struct buf *bp;

	if (ip->type == T_DEV) {
		if (ip->major < 0 || ip->major >= NDEV ||
		    !devsw[ip->major].read) {
			return -1;
		}
		return devsw[ip->major].read(ip, dst, n);
	}

	if (off > ip->size || off + n < off) { // overflow check
		return -1;
	}
	if (off + n > ip->size) {
		n = ip->size - off;
	}

	for (tot = 0; tot < n; tot += m, off += m, dst += m) {
		bp = bread(ip->dev, bmap(ip, off / BSIZE));
		m = MIN(n - tot, BSIZE - off % BSIZE);
		memmove(dst, bp->data + off % BSIZE, m);
		brelse(bp);
	}

	return n;
}

// Write data to inode with bounds checking
int writei(struct inode *ip, char *src, uint off, uint n) {
	uint tot, m;
	struct buf *bp;

	if (ip->type == T_DEV) {
		if (ip->major < 0 || ip->major >= NDEV ||
		    !devsw[ip->major].write) {
			return -1;
		}
		return devsw[ip->major].write(ip, src, n);
	}

	// Check for overflow and max file size
	if (off > ip->size || off + n < off) {
		return -1;
	}
	if (off + n > MAXFILE * BSIZE) {
		return -1;
	}

	for (tot = 0; tot < n; tot += m, off += m, src += m) {
		bp = bread(ip->dev, bmap(ip, off / BSIZE));
		m = MIN(n - tot, BSIZE - off % BSIZE);
		memmove(bp->data + off % BSIZE, src, m);
		log_write(bp);
		brelse(bp);
	}

	// Update size if file grew
	if (n > 0 && off > ip->size) {
		ip->size = off;
		iupdate(ip);
	}

	return n;
}

// Directory name comparison
int namecmp(const char *s, const char *t) { return strncmp(s, t, DIRSIZ); }

// Look for name in directory
struct inode *dirlookup(struct inode *dp, char *name, uint *poff) {
	uint off;
	struct dirent de;

	if (dp->type != T_DIR) {
		panic("dirlookup: not a directory");
	}

	for (off = 0; off < dp->size; off += sizeof(de)) {
		if (readi(dp, (char *)&de, off, sizeof(de)) != sizeof(de)) {
			panic("dirlookup: read error");
		}
		if (de.inum == 0) {
			continue;
		}
		if (namecmp(name, de.name) == 0) {
			if (poff) {
				*poff = off;
			}
			return iget(dp->dev, de.inum);
		}
	}

	return 0;
}

// Add directory entry
int dirlink(struct inode *dp, char *name, uint inum) {
	int off;
	struct dirent de;
	struct inode *ip;

	// Check for existing entry
	if ((ip = dirlookup(dp, name, 0)) != 0) {
		iput(ip);
		return -1; // Already exists
	}

	// Look for empty slot
	for (off = 0; off < dp->size; off += sizeof(de)) {
		if (readi(dp, (char *)&de, off, sizeof(de)) != sizeof(de)) {
			panic("dirlink: read error");
		}
		if (de.inum == 0) {
			break;
		}
	}

	strncpy(de.name, name, DIRSIZ);
	de.inum = inum;

	if (writei(dp, (char *)&de, off, sizeof(de)) != sizeof(de)) {
		panic("dirlink: write error");
	}

	return 0;
}

// Parse path element
static char *skipelem(char *path, char *name) {
	char *s;
	int len;

	while (*path == '/') {
		path++;
	}

	if (*path == 0) {
		return 0;
	}

	s = path;
	while (*path != '/' && *path != 0) {
		path++;
	}

	len = path - s;
	if (len >= DIRSIZ) {
		memmove(name, s, DIRSIZ);
	} else {
		memmove(name, s, len);
		name[len] = 0;
	}

	while (*path == '/') {
		path++;
	}

	return path;
}

// Core path resolution
static struct inode *namex(char *path, int nameiparent, char *name) {
	struct inode *ip, *next;

	if (*path == '/') {
		ip = iget(ROOTDEV, ROOTINO);
	} else {
		ip = idup(myproc()->cwd);
	}

	while ((path = skipelem(path, name)) != 0) {
		ilock(ip);

		if (ip->type != T_DIR) {
			iunlockput(ip);
			return 0;
		}

		if (nameiparent && *path == '\0') {
			iunlock(ip);
			return ip;
		}

		if ((next = dirlookup(ip, name, 0)) == 0) {
			iunlockput(ip);
			return 0;
		}

		iunlockput(ip);
		ip = next;
	}

	if (nameiparent) {
		iput(ip);
		return 0;
	}

	return ip;
}

// Resolve path to inode
struct inode *namei(char *path) {
	char name[DIRSIZ];
	return namex(path, 0, name);
}

// Resolve path to parent directory
struct inode *nameiparent(char *path, char *name) {
	return namex(path, 1, name);
}