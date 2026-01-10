#ifndef SLEEPLOCK_H
#define SLEEPLOCK_H

#include "spinlock.h"
#include "types.h"

struct sleeplock {
	uint locked;	    // Is the lock held?
	struct spinlock lk; // spinlock protecting this sleep lock
	char *name;	    // Name of lock.
	int pid;	    // Process holding lock
};

#endif