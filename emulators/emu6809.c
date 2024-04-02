/*
 *	Fake 6809 mini machine to run compiler tests
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "d6809.h"
#include "e6809.h"
#include "exec.h"

// Now visible globally for syscalls.c
uint8_t ram[65536];
int log_6809 = 0;

unsigned char e6809_read8(unsigned addr)
{
	return ram[addr];
}

unsigned char e6809_read8_debug(unsigned addr)
{
	if (addr < 0xFE00 || addr >= 0xFF00)
		return ram[addr];
	else
		return 0xFF;
}

static unsigned char fefcval=0;

void e6809_write8(unsigned addr, unsigned char val)
{
	int x;

	/* Writes to certain addresses act like system calls */
	/* 0xFEFF:  exit() with the val as the exit value */
	/* 0xFEFE:  putchar(val) */
	/* 0xFEFC/D: print out the 16-bit value as a decimal */

	switch(addr) {
	    case 0xFEFF:
		if (val == 0)
			exit(0);
		fprintf(stderr, "***FAIL %u\n", val);
		exit(1);
	    case 0xFEFE:
		putchar(val);
		break;
	    case 0xFEFD:
		/* Make the value signed */
		x= (fefcval << 8) | val;
		if (x>0x8000) x-= 0x10000;
		printf("%d\n", x);
		break;
	    case 0xFEFC:
		fefcval= val;	/* Save high byte for now */
		break;
	    default:
		ram[addr] = val;
	}
}

static const char *make_flags(uint8_t cc)
{
	static char buf[9];
	char *p = "EFHINZVC";
	char *d = buf;

	while (*p) {
		if (cc & 0x80)
			*d++ = *p;
		else
			*d++ = '-';
		cc <<= 1;
		p++;
	}
	*d = 0;
	return buf;
}

/* Called each new instruction issue */
void e6809_instruction(unsigned pc)
{
	char buf[80];
	struct reg6809 *r = e6809_get_regs();
	if (log_6809) {
		d6809_disassemble(buf, pc);
		fprintf(stderr, "%04X: %-16.16s | ", pc, buf);
		fprintf(stderr, "%s %02X:%02X %04X %04X %04X %04X\n", make_flags(r->cc), r->a, r->b, r->x, r->y, r->u, r->s);
	}
}

extern void set_initial_brk(int addr);

/* FUZIX executable header */
static struct exec E;

/* Load an executable into memory */
void load_executable(char *filename) {
	int fd;
	int cnt;
	int loadaddr;
	int bssend;

	/* Open the file */
	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		perror(filename);
		exit(1);
	}

	/* Read in a possble FUZIX header */
	cnt= read(fd, &E, sizeof(E));
	if (cnt == sizeof(E)) {
		/* Check the magic number and CPU */
		if ((ntohs(E.a_magic) == EXEC_MAGIC) && (E.a_cpu == A_6809)) {

			/* Determine the load address. */
			/* N.B. Add on the entry size so we */
			/* don't have to lseek back to the start */
			loadaddr= (E.a_base << 8) + E.a_entry;

			/* Determine the first address after the BSS */
			bssend= (E.a_base << 8) + ntohs(E.a_text) +
				ntohs(E.a_data) + ntohs(E.a_bss);
			set_initial_brk(bssend);

			/* Determine the start address */
			ram[0xFFFE] = loadaddr >> 8;
			ram[0xFFFF] = loadaddr & 0xff;

			/* Now read in the rest of the file */
			cnt=read(fd, &ram[loadaddr], 0xfff0);
			close(fd);
			return;
		}
	} 

	/* It's not a FUZIX binary, so read it in as a raw file at addr 0 */
	if (read(fd, ram, 0xFC00) < 10) {
		fprintf(stderr, "emu6809: executable not FUZIX, too small.\n");
		perror(filename);
		exit(1);
	}
	/* Set the starting address as 0x100 for a raw file */
	ram[0xFFFE] = 0x01;
	ram[0xFFFF] = 0x00;
	close(fd);
}

int main(int argc, char *argv[])
{
	if (!strcmp(argv[1], "-d")) {
		argv++;
		argc--;
		log_6809 = 1;
	}
	if (argc != 2) {
		fprintf(stderr, "Usage: emu6809 [-d] executable.\n");
		exit(1);
	}

	load_executable(argv[1]);

	e6809_reset(log_6809);
	while (1)
		e6809_sstep(0, 0);
	return 0;
}
