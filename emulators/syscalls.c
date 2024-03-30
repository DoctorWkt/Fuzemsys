// FUZIX syscall handler for the test emulators

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>

// FUZIX defines which could be different from the host system

#define FO_APPEND        4		// open(2) modes. The lowest
#define FO_SYNC          8		// two bits are fine.
#define FO_NDELAY        16
#define FO_CREAT         256
#define FO_EXCL          512
#define FO_TRUNC         1024
#define FO_NOCTTY        2048
#define FO_CLOEXEC       4096

// There are a set of "shim" functions to interface the syscall handler
// with each emulator. These are:
// - uint16_t get_sp(void);			// Get stack pointer value
// - uint8_t *get_memptr(uint16_t addr);	// Get pointer to mem location
// - char scarg(int off);			// Get signed char arg
// - unsigned char ucarg(int off);		// Get unsigned char arg
// - int siarg(int off);			// Get signed int arg
// - unsigned int uiarg(int off);		// Get unsigned int arg
//
// Each emulator needs its own shim functions.

#ifdef CPU_6809
#include "d6809.h"
#include "e6809.h"

// Emulator's memory
extern uint8_t ram[];

// Get the stack pointer value
uint16_t get_sp(void) {
  struct reg6809 *R= e6809_get_regs();
  return(R->s);
}

// Get a pointer to a location in the emulator's memory
// given the location's address
uint8_t *get_memptr(uint16_t addr) {
  return(&ram[addr]);
}

// Functions to read/write (un)signed char/int/long arguments at the
// "off"set on the stack. Offset 0 is the 1st byte of the 1st argument.
// For 6809, we use:
// extern unsigned char e6809_read8(unsigned address);
// extern void e6809_write8(unsigned address, unsigned char data);

// Signed 8-bit char argument
char scarg(int off) {
  uint16_t sp= get_sp() + 2 + off;
  char val= e6809_read8(sp);
  // printf("scarg %d is %d\n", off, val);
  return(val);
}

unsigned char ucarg(int off) {
  uint16_t sp= get_sp() + 2 + off;
  unsigned char val= e6809_read8(sp);
  // printf("ucarg %d is %d\n", off, val);
  return(val);
}

// Signed 16-bit integer argument
int siarg(int off) {
  uint16_t sp= get_sp() + 2 + off;
  int val= (e6809_read8(sp) << 8) | e6809_read8(sp+1);
  if (val > 0x8000) val -= 0x10000;
  // printf("siarg %d is %d\n", off, val);
  return(val);
}

// Unsigned 16-bit integer argument
unsigned int uiarg(int off) {
  uint16_t sp= get_sp() + 2 + off;
  unsigned int val= (e6809_read8(sp) << 8) | e6809_read8(sp+1);
  // printf("uiarg %d is %d\n", off, val);
  return(val);
}
#endif


// Get the syscall to perform and return the return value.
// If *longresult is 1, the result is 32-bits wide.
// If *longresult is 0, the result is 16-bits wide.
int do_syscall(int op, int *longresult) {
  int fd;		// File descriptor
  int mode;		// File mode
  int oflags, flags;	// File flags: FUZIX and host
  uint8_t *buf;		// Pointer to buffer
  const char *path;	// Pointer to pathname
  const char *newpath;	// Pointer to new pathname
  size_t cnt;		// Count in bytes
  int32_t *ooff;	// Lseek offset: FUZIX and host
  off_t off;
  int whence;		// Lseek whence
  int result;		// Native syscall result
  int32_t sres;		// Emulator signed result

  *longresult=0;	// Assume a 16-bit result

  switch(op) {
    case 0:		// _exit
	_exit(siarg(0));
    case 1:		// open
	path= (const char *)get_memptr(uiarg(0));
	oflags= uiarg(2);
	mode= uiarg(4);

	// Map the FUZIX flags to the host system.
	// Keep the lowest two bits.
	flags=   oflags & 0x3;
	flags |= (oflags & FO_APPEND)  ? O_APPEND : 0;
	flags |= (oflags & FO_SYNC)    ? O_SYNC : 0;
	flags |= (oflags & FO_NDELAY)  ? O_NDELAY : 0;
	flags |= (oflags & FO_CREAT)   ? O_CREAT : 0;
	flags |= (oflags & FO_EXCL)    ? O_EXCL : 0;
	flags |= (oflags & FO_TRUNC)   ? O_TRUNC : 0;
	flags |= (oflags & FO_NOCTTY)  ? O_NOCTTY : 0;
	flags |= (oflags & FO_CLOEXEC) ? O_CLOEXEC : 0;
	result= open(path, flags, mode);
	sres= (int32_t)(result & 0xffff);
	return(sres);
    case 2:		// close
	fd= uiarg(0);
	result= close(fd);
	sres= (int32_t)(result & 0xffff);
	return(sres);
    case 3:		// rename
	path=    (const char *)get_memptr(uiarg(0));
	newpath= (const char *)get_memptr(uiarg(1));
	result= rename(path, newpath);
	sres= (int32_t)(result & 0xffff);
	return(sres);
    case 5:		// link
	path=    (const char *)get_memptr(uiarg(0));
	newpath= (const char *)get_memptr(uiarg(1));
	result= link(path, newpath);
	sres= (int32_t)(result & 0xffff);
	return(sres);
    case 6:		// unlink
	path=    (const char *)get_memptr(uiarg(0));
	result= unlink(path);
	sres= (int32_t)(result & 0xffff);
	return(sres);
    case 7:		// read
	fd= uiarg(0);
	buf= get_memptr(uiarg(2));
	cnt= uiarg(4);
	result= read(fd, buf, cnt);
	sres= (int32_t)(result & 0xffff);
	return(sres);
    case 8:		// write
	fd= uiarg(0);
	buf= get_memptr(uiarg(2));
	cnt= uiarg(4);
	result= write(fd, buf, cnt);
	sres= (int32_t)(result & 0xffff);
	return(sres);
    case 9:		// _lseek
  	*longresult=1;	// 32-bit result
	fd= uiarg(0);
	ooff= (int32_t *)get_memptr(uiarg(1));
	whence= uiarg(1);
	// Convert FUZIX offset to host endian
	off= ntohl(*ooff);
	off= lseek(fd, off, whence);
	sres= (int32_t)(off & 0xffffffff);
	return(sres);
    default: fprintf(stderr, "Unhandled syscall %d\n", op); exit(1);
  }
  return(0);
}
