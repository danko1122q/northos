#ifndef ELF_H
#define ELF_H

#include "types.h"

#define ELF_MAGIC 0x464C457FU

struct elfhdr {
	uint magic;
	uchar elf[12];
	ushort type;
	ushort machine;
	uint version;
	uint entry;
	uint phoff;
	uint shoff;
	uint flags;
	ushort ehsize;
	ushort phentsize;
	ushort phnum;
	ushort shentsize;
	ushort shnum;
	ushort shstrndx;
};

struct proghdr {
	uint type;
	uint off;
	uint vaddr;
	uint paddr;
	uint filesz;
	uint memsz;
	uint flags;
	uint align;
};

#define ELF_PROG_LOAD 1
#define ELF_PROG_FLAG_EXEC 1
#define ELF_PROG_FLAG_WRITE 2
#define ELF_PROG_FLAG_READ 4

#endif // ELF_H