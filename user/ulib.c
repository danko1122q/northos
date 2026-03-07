#include "fcntl.h"
#include "stat.h"
#include "types.h"
#include "user.h"
#include "x86.h"

char *strcpy(char *s, const char *t) {
	char *os;

	os = s;
	while ((*s++ = *t++) != 0)
		;
	return os;
}

int strcmp(const char *p, const char *q) {
	while (*p && *p == *q)
		p++, q++;
	return (uchar)*p - (uchar)*q;
}

uint strlen(const char *s) {
	int n;

	for (n = 0; s[n]; n++)
		;
	return n;
}

void *memset(void *dst, int c, uint n) {
	stosb(dst, c, n);
	return dst;
}

char *strchr(const char *s, char c) {
	for (; *s; s++)
		if (*s == c)
			return (char *)s;
	return 0;
}

char *gets(char *buf, int max) {
	int i, cc;
	char c;

	for (i = 0; i + 1 < max;) {
		cc = read(0, &c, 1);
		if (cc < 1)
			break;
		buf[i++] = c;
		if (c == '\n' || c == '\r')
			break;
	}
	buf[i] = '\0';
	return buf;
}

int stat(const char *n, struct stat *st) {
	int fd;
	int r;

	fd = open(n, O_RDONLY);
	if (fd < 0)
		return -1;
	r = fstat(fd, st);
	close(fd);
	return r;
}

int atoi(const char *s) {
	int n;

	n = 0;
	while ('0' <= *s && *s <= '9')
		n = n * 10 + *s++ - '0';
	return n;
}

void *memmove(void *vdst, const void *vsrc, int n) {
	char *dst;
	const char *src;

	dst = vdst;
	src = vsrc;
	while (n-- > 0)
		*dst++ = *src++;
	return vdst;
}

char *strcat(char *dest, const char *src) {
	char *ret = dest;
	while (*dest)
		dest++;
	while ((*dest++ = *src++))
		;
	return ret;
}

// Check if the RDRAND instruction is available via CPUID leaf 1, ECX bit 30.
int RDRAND_available(void) {
	uint ecx;
	asm volatile(
		"movl $1, %%eax\n\t"
		"cpuid\n\t"
		: "=c"(ecx)
		:
		: "eax", "ebx", "edx"
	);
	return (ecx >> 30) & 1;
}

// Multiply-with-carry PRNG using two 32-bit words and multiplier 4294967118.
// Seeded with RDRAND if available, otherwise with fixed constants.
uint rand(void) {
	static uint w = 0;
	static uint z = 0;
	static int seeded = 0;

	if (!seeded) {
		if (RDRAND_available()) {
			uint r1 = 0, r2 = 0;
			// Loop until the carry flag signals a valid random value.
			asm volatile(
				"1: rdrand %0\n\t"
				"jnc 1b\n\t"
				: "=r"(r1)
			);
			asm volatile(
				"1: rdrand %0\n\t"
				"jnc 1b\n\t"
				: "=r"(r2)
			);
			w = r1 ? r1 : 362436069;
			z = r2 ? r2 : 521288629;
		} else {
			w = 362436069;
			z = 521288629;
		}
		seeded = 1;
	}

	// Multiply-with-carry iteration with multiplier 4294967118.
	z = 4294967118U * (z & 0xFFFF) + (z >> 16);
	w = 4294967118U * (w & 0xFFFF) + (w >> 16);
	return (z << 16) + (w & 0xFFFF);
}
