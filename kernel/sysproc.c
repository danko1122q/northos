#include "date.h"
#include "defs.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"
#include "proc.h"
#include "types.h"
#include "x86.h"
#include "rtc.h"

int sys_fork(void) { return fork(); }

int sys_exit(void) {
	exit();
	return 0; // not reached
}

int sys_wait(void) { return wait(); }

int sys_kill(void) {
	int pid;

	if (argint(0, &pid) < 0)
		return -1;
	return kill(pid);
}

int sys_getpid(void) { return myproc()->pid; }

int sys_sbrk(void) {
	int addr;
	int n;

	if (argint(0, &n) < 0)
		return -1;
	addr = myproc()->sz;
	if (growproc(n) < 0)
		return -1;
	return addr;
}

int sys_sleep(void) {
	int n;
	uint ticks0;

	if (argint(0, &n) < 0)
		return -1;
	acquire(&tickslock);
	ticks0 = ticks;
	while (ticks - ticks0 < n) {
		if (myproc()->killed) {
			release(&tickslock);
			return -1;
		}
		sleep(&ticks, &tickslock);
	}
	release(&tickslock);
	return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int sys_uptime(void) {
	uint xticks;

	acquire(&tickslock);
	xticks = ticks;
	release(&tickslock);
	return xticks;
}

// System call untuk mematikan komputer (QEMU)
int sys_halt(void) {
	// Instruksi khusus untuk mematikan QEMU
	// 0x604 adalah port acpi, 0x2000 adalah perintah poweroff
	outw(0x604, 0x2000);

	// Jika kode di atas gagal (untuk versi QEMU lama)
	outw(0xB004, 0x2000);

	return 0;
}

// System call untuk restart komputer
int sys_reboot(void) {
	// Mengirim sinyal reset ke keyboard controller (standar x86 reboot)
	uchar good = 0x02;
	while (good & 0x02)
		good = inb(0x64);
	outb(0x64, 0xFE);

	return 0;
}

// Implementasi System Call RTC yang sudah diperbaiki cast-nya
int sys_get_rtc_time(void) {
	int *h, *m, *s;

	// Ubah cast menjadi (char **) agar sesuai dengan defs.h
	if (argptr(0, (char **)&h, sizeof(int)) < 0 ||
	    argptr(1, (char **)&m, sizeof(int)) < 0 ||
	    argptr(2, (char **)&s, sizeof(int)) < 0)
		return -1;

	rtc_read_time(h, m, s);
	return 0;
}

int sys_get_rtc_date(void) {
	int *d, *mo, *y;

	// Ubah cast menjadi (char **) agar sesuai dengan defs.h
	if (argptr(0, (char **)&d, sizeof(int)) < 0 ||
	    argptr(1, (char **)&mo, sizeof(int)) < 0 ||
	    argptr(2, (char **)&y, sizeof(int)) < 0)
		return -1;

	rtc_read_date(d, mo, y);
	return 0;
}