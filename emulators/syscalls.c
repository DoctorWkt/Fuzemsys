// FUZIX syscall handler for the test emulators
//
// If you are porting to another emulator, scroll
// down and read the PORTING guide.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <endian.h>
#include <time.h>
#include <errno.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_ARGS	200	// Max cmd-line args per process

// FUZIX defines which could be different from the host system

#define FO_APPEND        4		// open(2) modes. The lowest
#define FO_SYNC          8		// two bits are fine.
#define FO_NDELAY        16
#define FO_CREAT         256
#define FO_EXCL          512
#define FO_TRUNC         1024
#define FO_NOCTTY        2048
#define FO_CLOEXEC       4096

#define FO_TCGETS	1
#define FO_TCSETS	2
#define FO_TCSETSW	3
#define FO_TCSETSF	4
#define FO_TIOCFLUSH	6
#define FO_TIOCHANGUP	7
#define FO_TIOCOSTOP	8
#define FO_TIOCOSTART	9
#define FO_TIOCGWINSZ	10
#define FO_TIOCGPGRP	12

// The stat structure used by FUZIX
struct _uzistat
{
        int16_t  st_dev;
        uint16_t st_ino;
        uint16_t st_mode;
        uint16_t st_nlink;
        uint16_t st_uid;
        uint16_t st_gid;
        uint16_t st_rdev;
        uint32_t st_size;
        uint32_t st__atime;
        uint32_t st__mtime;
        uint32_t st__ctime;
        uint32_t st_timeh;      /* Time high bytes */
};

// FUZIX dirent structure
struct _dirent {
        uint16_t d_ino;
        char     d_name[30];
};

// FUZIX termios stuff
typedef uint16_t fotcflag_t;
typedef uint16_t fospeed_t;
typedef uint8_t focc_t;

#define FONCCS 12
struct fotermios {
  fotcflag_t c_iflag;
  fotcflag_t c_oflag;
  fotcflag_t c_cflag;
  fotcflag_t c_lflag;
  focc_t c_cc[FONCCS];
};

struct fowinsize {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
};

// PORTING TO ANOTHER EMULATOR
//
// First off, make a -D define for your platform, e.g. -DCPU_6809.
// Then do an #ifdef MYPLATFORM, #define EMU_BIGENDIAN or #define EMU_LITTLEENDIAN, #endif
// as the code will need to convert from emulator endian <-> host endian.
//
// Now, define a set of "shim" functions to interface the syscall handler here with
// with your emulator. These are:
//
// - uint16_t get_sp(void);			// Get stack pointer value
// - uint8_t *get_memptr(uint16_t addr);	// Get ptr to mem location or NULL if addr 0
// - char scarg(int off);			// Get signed char arg
// - unsigned char ucarg(int off);		// Get unsigned char arg
// - int siarg(int off);			// Get signed int arg
// - unsigned int uiarg(int off);		// Get unsigned int arg
// - void putui(uint16_t addr, uint16_t val)	// Put 16-bit value in mem at the given addr
// - uint16_t getui(uint16_t addr)		// Get 16-bit value in emulator mem at addr
//
// Finally, add calls to these functions here in your emulator's code:
//
// - int do_syscall(int op, int *longresult)	// Do a system call. See comments below
// - void set_initial_brk(uint16_t addr)	// Set the initial brk value
// - void set_fuzix_root(char *dirname)		// Set the host directory to be the
//						// emulated root directory
// - int set_arg_env()				// Put args on the emulated stack.
//						// See comments below

#ifdef CPU_6809
#include "d6809.h"
#include "e6809.h"
#define EMU_BIGENDIAN

// Emulator's memory
extern uint8_t ram[];

// Get the stack pointer value
uint16_t get_sp(void) {
  struct reg6809 *R= e6809_get_regs();
  return(R->s);
}

// Get a pointer to a location in the emulator's memory
// given the location's address. Return NULL is the
// emulator's address is zero.
uint8_t *get_memptr(uint16_t addr) {
  if (addr==0) return(NULL);
  return(&ram[addr]);
}

// Functions to read/write (un)signed char/int/long arguments at the
// "off"set on the stack. Offset 0 is the 1st byte of the 1st argument.

// Signed 8-bit char argument
char scarg(int off) {
  uint16_t sp= get_sp() + 2 + off;
  char val= e6809_read8(sp);
  // printf("scarg %d is %d\n", off, val);
  return(val);
}

// Unsigned 8-bit char argument
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

// Put 16-bit value in memory at the given location
void putui(uint16_t addr, uint16_t val) {
  e6809_write8(addr++, val >> 8);
  e6809_write8(addr, val & 0xff);
}

// Get 16-bit value in memory at the given location
uint16_t getui(uint16_t addr) {
  return((e6809_read8(addr) << 8) | e6809_read8(addr+1));
}
#endif

#ifdef CPU_Z80
#include "libz80/z80.h"
#include "emuz80.h"
#define EMU_LITTLEENDIAN

// Emulator's memory
extern uint8_t ram[];

// Get the stack pointer value
uint16_t get_sp(void) {
  return(cpu_z80.R1.wr.SP);
}

// Get a pointer to a location in the emulator's memory
// given the location's address. Return NULL is the
// emulator's address is zero.
uint8_t *get_memptr(uint16_t addr) {
  if (addr==0) return(NULL);
  return(&ram[addr]);
}

// Functions to read/write (un)signed char/int/long arguments at the
// "off"set on the stack. Offset 0 is the 1st byte of the 1st argument.

// Signed 8-bit char argument
char scarg(int off) {
  uint16_t sp= get_sp() + 2 + off;
  char val= mem_read(0, sp);
  // printf("scarg %d is %d\n", off, val);
  return(val);
}

// Unsigned 8-bit char argument
unsigned char ucarg(int off) {
  uint16_t sp= get_sp() + 2 + off;
  unsigned char val= mem_read(0, sp);
  // printf("ucarg %d is %d\n", off, val);
  return(val);
}

// Signed 16-bit integer argument
int siarg(int off) {
  uint16_t sp= get_sp() + 2 + off;
  int val= mem_read(0, sp) | (mem_read(0, sp+1) << 8);
  if (val > 0x8000) val -= 0x10000;
  // printf("siarg %d is %d\n", off, val);
  return(val);
}

// Unsigned 16-bit integer argument
unsigned int uiarg(int off) {
  uint16_t sp= get_sp() + 2 + off;
  unsigned int val= mem_read(0, sp) | (mem_read(0, sp+1) << 8);
  // printf("uiarg %d is %d\n", off, val);
  return(val);
}

// Put 16-bit value in memory at the given location
void putui(uint16_t addr, uint16_t val) {
  mem_write(0, addr++, val & 0xff);
  mem_write(0, addr, val >> 8);
}

// Get 16-bit value in memory at the given location
uint16_t getui(uint16_t addr) {
  return(mem_read(0, addr) | (mem_read(0, addr+1) <<8));
}
#endif

// Determine which endian functions we will use
#ifdef EMU_BIGENDIAN
  #define htoemu16 htobe16
  #define emu16toh be16toh
  #define htoemu32 htobe32
  #define emu32toh be32toh
  #define htoemu64 htobe64
  #define emu64toh be64toh
#endif
#ifdef EMU_LITTLEENDIAN
  #define htoemu16 htole16
  #define emu16toh le16toh
  #define htoemu32 htole32
  #define emu32toh le32toh
  #define htoemu64 htole64
  #define emu64toh le64toh
#endif


                                /* The following two buffers are used as */
                                /* part of the translation from virtal */
                                /* absolute filenames to native ones. We */
                                /* only have 2 buffers, so if you call */
                                /* xlate_filename() 3 times, the 1st return */
                                /* value will be destroyed. */
static char realfilename[2][2 * PATH_MAX];
static char *rfn[2];
static int whichrfn=0;

/* Translate from a filename to one which is possibly rooted in $FUZIXROOT.
 * Note we return a pointer to one of two buffers. The caller does not
 * have to free the returned pointer, but successive calls will destroy
 * calls from >2 calls earlier.
 */
static char *xlate_filename(char *name)
{
    int i=whichrfn;

    if (name == NULL) return (NULL);
    if (name[0] != '/') return (name);  /* Relative, keep it relative */
    strcpy(rfn[i], name);               /* Copy name into buffer */
    whichrfn= 1 - whichrfn;             /* Switch to other buffer next time */
    return (realfilename[i]);
}

void set_fuzix_root(char *dirname)
{

  // Test if the dirname exists if not ""
  if (strlen(dirname)!=0) {
    DIR *D= opendir(dirname);
    if (D==NULL) {
      fprintf(stderr, "Unable to use FUZIXROOT %s: %s\n",
	  dirname, strerror(errno));
      exit(1);
    }
    closedir(D);
  }
  strcpy(realfilename[0], dirname);      
  strcpy(realfilename[1], dirname);      
  rfn[0] = realfilename[0]; rfn[0] += strlen(realfilename[0]);
  rfn[1] = realfilename[1]; rfn[1] += strlen(realfilename[1]);
}

//
// Arguments and the environment. When an executable is loaded,
// the system sets up the stack as follows:
//
//	_________________________________
//	| (NULL)			| top of memory
//	|-------------------------------|
//	|				|
//	| environment strings		|
//	|				|
//	|-------------------------------|
//	|				|
//	| argument strings		|
//	|				|
//	|-------------------------------|
//	| envp[envc] (NULL)		| end of environment vector tag, a 0
//	|-------------------------------|
//	| envp[envc-1]			| pointer to last environment string
//	|-------------------------------|
//	| ...				|
//	|-------------------------------|
//	| envp[0]			| pointer to first environment string
//	|-------------------------------|
//	| argv[argc] (NULL)		| end of argument vector tag, a 0
//	|-------------------------------|
//	| argv[argc-1]			| pointer to last argument string
//	|-------------------------------|
//	| ...				|
//	|-------------------------------|
//	| argv[0]			| pointer to first argument string
//	|-------------------------------|
// sp-> | argc				| number of arguments
//	---------------------------------
//
// Return the new stack pointer value.


// Build an envp[] and argv[] array below the sp value.
// Put the arc, argv pointer and envp pointer on the stack.
// Return the new stack pointer value.
int set_arg_env(uint16_t sp, char **argv, char **envp)
{
  int i;
  uint16_t argc, envc;
  uint16_t argvposn, envpposn;
  uint16_t posn, len;
  uint8_t *cptr;
  uint16_t eposn[MAX_ARGS];
  uint16_t aposn[MAX_ARGS];
  						// Determine argc and envc.
  for (i=0, argc=0; argv[i]!=NULL; i++) argc++;
  for (i=0, envc=0; envp[i]!=NULL; i++) envc++;

#ifdef DEBUG
  fprintf(stderr, "In set_arg_env, argc is %d\n", argc);
  for (i = 0; i < argc; i++)
    fprintf(stderr, "  argv[%d] is %s\n", i, argv[i]);
  for (i = 0; i < envc; i++)
    fprintf(stderr, "  envp[%d] is %s\n", i, envp[i]);
#endif

  						// Now build the arguments and
						// pointers on the stack
  posn = sp - 2;
  putui(posn, 0);				// Put a NULL on top of stack

  for (i = envc - 1; i != -1; i--) {		// For each env string
    len = strlen(envp[i]) + 1;			// get its length
    posn -= len;
    cptr= get_memptr(posn);
    memcpy(cptr, envp[i], (size_t) len);	// and copy the string
    eposn[i] = posn;				// onto the stack
  }

  for (i = argc - 1; i != -1; i--) {		// For each arg string
    len = strlen(argv[i]) + 1;			// get its length
    posn -= len;
    cptr= get_memptr(posn);
    memcpy(cptr, argv[i], (size_t) len);	// and copy the string
    aposn[i] = posn;				// onto the stack
  }

  posn -= 2;
  putui(posn, 0);				// Put a NULL at end of env array

  for (i = envc - 1; i != -1; i--) {		// For each envp string
    posn -= 2; 					// put a ptr to the string on the stack
    putui(posn, (u_int16_t) eposn[i]);
  }
  envpposn= posn;				// Save position of first pointer

  posn -= 2;
  putui(posn, 0);				// Put a NULL before arg ptrs

  for (i = argc - 1; i != -1; i--) {		// For each arg string
    posn -= 2;
    putui(posn, (u_int16_t) aposn[i]);		// put a ptr to the string on the stack
  }
  argvposn= posn;				// Save position of first pointer

  posn -= 2;
  putui(posn, (u_int16_t) envpposn);		// Save ptr to envp list of pointers
  posn -= 2;
  putui(posn, (u_int16_t) argvposn);		// Save ptr to argv list of pointers
  posn -= 2;
  putui(posn, (u_int16_t) argc);		// Save the count of args
  return(posn);
}

// Initial and current brk value;
static uint16_t curbrk=0;
static uint16_t initbrk=0;

// We use these to translate the emulated system's
// argv pointers into pointers into our memory
char *arglist[MAX_ARGS];

// Set the initial break point. This is usually
// after the end of the BSS.
void set_initial_brk(uint16_t addr) {
  curbrk=initbrk=addr;
}

// Copy the host stat information into a _uzistat buffer
// in the emulator's memory
static void copystat(struct stat *src, struct _uzistat *dst) {
        dst->st_dev= htoemu16(src->st_dev & 0xffff);
        dst->st_ino= htoemu16(src->st_ino & 0xffff);
        dst->st_mode= htoemu16(src->st_mode & 0xffff);
        dst->st_nlink= htoemu16(src->st_nlink & 0xffff);
        dst->st_uid= htoemu16(src->st_uid & 0xffff);
        dst->st_gid= htoemu16(src->st_gid & 0xffff);
        dst->st_rdev= htoemu16(src->st_rdev & 0xffff);
        dst->st_size= htoemu16(src->st_size & 0xffff);
        dst->st__atime= htoemu16(src->st_atime & 0xffff);
        dst->st__mtime= htoemu16(src->st_mtime & 0xffff);
        dst->st__ctime= htoemu16(src->st_ctime & 0xffff);
}

// Determine if the path is a directory. If so, read the
// directory and create a temporary file which contains
// _dirent records that match the directory. Unlink the
// file so it goes away on a close(). Return the opened fd.
int make_dirent_file(const char *path) {
  DIR *Dir;
  struct dirent *Dirent;
  struct _dirent fuzdent;
  int fd;
  char filename[]= "/tmp/syscall.XXXXXX";

  // Can we open it as a directory?
  Dir= opendir(path);
  if (Dir==NULL)
    return(-1);

  // Make a temporary file
  fd= mkstemp(filename);
  if (fd==-1) {
    closedir(Dir); return(-1);
  }

  // Read directory entries and build the _dirent entries
  while ((Dirent = readdir(Dir)) != NULL) {
    fuzdent.d_ino= htoemu16(Dirent->d_ino & 0xffff);
    strncpy(fuzdent.d_name, Dirent->d_name, 30);
    write(fd, &fuzdent, sizeof(fuzdent));
  }

  // Close the dir, rewind and unlink the file, return the fd
  lseek(fd, SEEK_SET, 0);
  closedir(Dir); unlink(filename);
  return(fd);
}

// Get the syscall to perform and return the return value.
// Sets the host errno to 0, or non-zero on error.
// If *longresult is 1, the result is 32-bits wide.
// If *longresult is 0, the result is 16-bits wide.
int do_syscall(int op, int *longresult) {
  int i;
  int fd, newfd;	// File descriptor and new fd
  int sig;		// Signal
  uint16_t sp;		// Current stack pointer
  uint16_t brkval;	// New brk value
  uint16_t oldbrkval;	// Old brk value
  uint16_t duration;	// Duration of time
  mode_t mode;		// File mode
  int32_t i32;		// Generic signed 32-bit value
  int oflags, flags;	// File flags: FUZIX and host
  uint8_t *buf;		// Pointer to buffer
  const char *path;	// Pointer to pathname
  const char *newpath;	// Pointer to new pathname
  size_t cnt;		// Count in bytes
  int32_t *ooff;	// Lseek offset: FUZIX and host
  off_t off;
  uid_t owner;		// Chown owner and group
  gid_t group;
  int whence;		// Lseek whence
  int result;		// Native syscall result
  int32_t sres;		// Emulator signed result
  time_t tim;		// Time value
  int32_t *ktim;	// Pointer to FUZIX ktime struct. Assumes NO_64BIT
  pid_t pid, pgid;	// Process id
  int16_t *iptr;	// Pointer to integer
  uint16_t addr;	// Address in emulator memory
  int options;		// Waitpid options
  int wstatus;		// Waitpid status
  struct stat hstat;	// Host stat struct;
  struct _uzistat *ustat; // Emulator stat struct;
  int pipefd[2];	// Pipe fds
  struct termios termios;	// Our termios struct
  struct fotermios *ftios;	// Pointer to FUZIX termios struct
  struct winsize w;	// Host window size
  struct fowinsize *fw;	// FUZIX window size

  *longresult=0;	// Assume a 16-bit result
  errno= 0;		// Start with no syscall errors

  switch(op) {
    case 0:		// _exit
	_exit(siarg(0));
    case 1:		// open
	path= (const char *)xlate_filename((char *)get_memptr(uiarg(0)));
	if (path==NULL) { result=-1; errno=EFAULT; break; }
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

	// If this is a directory, then we make a file that contains
	// the contents of the directory in _dirent format. If not,
	// do a normal open()
	if ((result= make_dirent_file(path)) == -1)
	  result= open(path, flags, mode);
	break;
    case 2:		// close
	fd= uiarg(0);
	result= close(fd);
	break;
    case 3:		// rename
	path=    (const char *)xlate_filename((char *)get_memptr(uiarg(0)));
	if (path==NULL) { result=-1; errno=EFAULT; break; }
	newpath= (const char *)xlate_filename((char *)get_memptr(uiarg(2)));
	if (newpath==NULL) { result=-1; errno=EFAULT; break; }
	result= rename(path, newpath);
	break;
    case 5:		// link
	path=    (const char *)xlate_filename((char *)get_memptr(uiarg(0)));
	if (path==NULL) { result=-1; errno=EFAULT; break; }
	newpath= (const char *)xlate_filename((char *)get_memptr(uiarg(2)));
	if (newpath==NULL) { result=-1; errno=EFAULT; break; }
	result= link(path, newpath);
	break;
    case 6:		// unlink
	path=    (const char *)xlate_filename((char *)get_memptr(uiarg(0)));
	if (path==NULL) { result=-1; errno=EFAULT; break; }
	result= unlink(path);
	break;
    case 7:		// read
	fd= uiarg(0);
	buf= get_memptr(uiarg(2));
	if (buf==NULL) { result=-1; errno=EFAULT; break; }
	cnt= uiarg(4);
	result= read(fd, buf, cnt);
	break;
    case 8:		// write
	fd= uiarg(0);
	buf= get_memptr(uiarg(2));
	if (buf==NULL) { result=-1; errno=EFAULT; break; }
	cnt= uiarg(4);
	result= write(fd, buf, cnt);
	break;
    case 9:		// _lseek
	fd= uiarg(0);
	ooff= (int32_t *)get_memptr(uiarg(2));
	if (ooff==NULL) { result=-1; errno=EFAULT; break; }
	whence= uiarg(4);
	// Convert FUZIX offset to host endian
	i32= emu32toh(*ooff);
	off= i32;
	off= lseek(fd, off, whence);
	// Convert result back to FUZIX endian
	*ooff= htoemu32((int32_t)(off & 0xffffffff));
	// Return -1 on error, 0 otherwise
	if (off==-1)
	  return(-1);
	return(0);
    case 10:		// chdir
	path= (const char *)xlate_filename((char *)get_memptr(uiarg(0)));
	if (path==NULL) { result=-1; errno=EFAULT; break; }
	result= chdir(path);
	break;
    case 11:		// sync
	sync();
	return(0);
    case 12:		// access
	path= (const char *)xlate_filename((char *)get_memptr(uiarg(0)));
	if (path==NULL) { result=-1; errno=EFAULT; break; }
	mode= uiarg(2);
	result= access(path, mode);
	break;
    case 13:		// chmod
	path= (const char *)xlate_filename((char *)get_memptr(uiarg(0)));
	if (path==NULL) { result=-1; errno=EFAULT; break; }
	mode= uiarg(2);
	result= chmod(path, mode);
	break;
    case 14:		// chown
	path= (const char *)xlate_filename((char *)get_memptr(uiarg(0)));
	if (path==NULL) { result=-1; errno=EFAULT; break; }
 	owner= uiarg(2);
 	group= uiarg(4);
	result= chown(path, owner, group);
	break;
    case 15:		// _stat
	path= (const char *)xlate_filename((char *)get_memptr(uiarg(0)));
	if (path==NULL) { result=-1; errno=EFAULT; break; }
	ustat= (struct _uzistat *)get_memptr(uiarg(2));
	if (ustat==NULL) { result= -1; errno= EFAULT; break; }
	result= stat(path, &hstat);
	if (result==-1) break;
	copystat(&hstat, ustat);
	break;
    case 16:		// _fstat
	fd= uiarg(0);
	ustat= (struct _uzistat *)get_memptr(uiarg(2));
	if (ustat==NULL) { result= -1; errno= EFAULT; break; }
	result= fstat(fd, &hstat);
	if (result==-1) break;
	copystat(&hstat, ustat);
	break;
    case 17:		// dup
	fd= uiarg(0);
	result= dup(fd);
	break;
    case 18:		// getpid
	result= getpid();
	break;
    case 19:		// getppid
	result= getppid();
	break;
    case 20:		// getuid
	result= getuid();
	break;
    case 21:		// umask
	mode= uiarg(0);
	result= umask(mode);
	break;
    case 23:		// execve
	path= (const char *)xlate_filename((char *)get_memptr(uiarg(0)));
	if (path==NULL) { result=-1; errno=EFAULT; break; }
	// Get address of base of arg list
	addr= uiarg(2);
	// Get the pointers to the arguments
	for (i=0; getui(addr)!=0; i++, addr+=2)
	  arglist[i]= (char *)get_memptr(getui(addr));
	// NULL terminate the list
	arglist[i]=NULL;
	result=execve(path, arglist, NULL);
	break;
    case 25:		// setuid
	owner= uiarg(0);
	result= setuid(owner);
	break;
    case 26:		// setgid
	group= uiarg(0);
	result= setgid(group);
	break;
    case 27:		// _time, assumes NO_64BIT
	ktim= (int32_t *)get_memptr(uiarg(0));
	if (ktim==NULL) { result=-1; errno=EFAULT; break; }
	tim= time(NULL);
	// Convert to FUZIX endian
	*ktim= htoemu32((int32_t)tim & 0xffffffff);
	return(0);
    case 29:		// ioctl. Only a few implemented
	fd= uiarg(0);
	options= uiarg(2);
	switch (options) {
	  case FO_TCGETS:
	    ftios= (struct fotermios *)get_memptr(uiarg(4));
	    if (ftios==NULL) { result=-1; errno=EFAULT; break; }
	    result= tcgetattr(fd, &termios);
	    if (result== -1) break;
	    ftios->c_iflag= htoemu16(termios.c_iflag);
	    ftios->c_oflag= htoemu16(termios.c_oflag);
	    ftios->c_cflag= htoemu16(termios.c_cflag);
	    ftios->c_lflag= htoemu16(termios.c_lflag);
	    for (i=0; i< FONCCS; i++)
	      ftios->c_cc[i]= termios.c_cc[i];
	    break;
	  case FO_TCSETS:
	    ftios= (struct fotermios *)get_memptr(uiarg(4));
	    if (ftios==NULL) { result=-1; errno=EFAULT; break; }
	    termios.c_iflag= emu16toh(ftios->c_iflag);
	    termios.c_oflag= emu16toh(ftios->c_oflag);
	    termios.c_cflag= emu16toh(ftios->c_cflag);
	    termios.c_lflag= emu16toh(ftios->c_lflag);
	    for (i=0; i< FONCCS; i++)
	      termios.c_cc[i]= ftios->c_cc[i];
	    result= tcgetattr(fd, &termios);
	    break;
	  case FO_TIOCOSTOP:
	    result= ioctl(fd, TCOOFF);
	    break;
	  case FO_TIOCOSTART:
	    result= ioctl(fd, TCOON);
	    break;
	  case FO_TIOCHANGUP:
	    options= uiarg(4);
	    fprintf(stderr, "TIOCHANGUP not yet implemented\n");
	    result= 0;
	    break;
	  case FO_TIOCFLUSH:
	    options= uiarg(4);
	    result= tcflush(fd, options);
	    break;
	  case FO_TCSETSW:
	    result= tcdrain(fd);
	    break;
	  case FO_TCSETSF:
	    result= tcflush(fd, TCOFLUSH);
	    break;
	  case FO_TIOCGPGRP:
	    result= isatty(fd);
	    break;
	  case FO_TIOCGWINSZ:
	    fw= (struct fowinsize *)get_memptr(uiarg(4));
	    if (fw==NULL) { result=-1; errno=EFAULT; break; }
	    result= ioctl(fd, TIOCGWINSZ, &w);
	    if (result == -1) break;
    	    fw->ws_row= htoemu16(w.ws_row);
    	    fw->ws_col= htoemu16(w.ws_col);
	    break;
	  default: fprintf(stderr, "Unimplemented ioctl %d\n", options); exit(1);
	}
	break;
    case 30:		// brk
	brkval= uiarg(0);
	sp= get_sp();
	// printf("In brk, request 0x%x, initbrk 0x%x, sp 0x%x\n", brkval, initbrk, sp);
	// If below the initial brk or
	// in the stack, return an error
	if ((brkval < initbrk) || (brkval >= sp)) {
	  errno= ENOMEM;
	  result= -1;
	} else {
	  curbrk= brkval;
	  result= 0;
	}
	// printf("In brk, result is %d\n", result);
	break;
    case 31:		// sbrk
	sp= get_sp();
	oldbrkval= curbrk;
	brkval= curbrk + siarg(0);
	// printf("In sbrk, request 0x%x, initbrk 0x%x, sp 0x%x\n", siarg(0), initbrk, sp);
	if ((brkval < initbrk) || (brkval >= sp)) {
	  errno= ENOMEM;
	  result= -1;
	} else {
	  curbrk= brkval;
	  result= oldbrkval;
	}
	// printf("In sbrk, result is 0x%x\n", result);
	break;
    case 32:		// _fork
	result= fork();
	break;
    case 36:		// dup2
	fd= uiarg(0);
	newfd= uiarg(2);
	result= dup2(fd, newfd);
	break;
    case 37:		// _pause
	// If argument is zero, we do a pause().
	// Otherwise do a sleep in tenths of a second
	duration= uiarg(0);
	if (duration)
	  result= usleep(duration * 1000000);
	else {
	  pause();
	  result=0;
	}
    case 39:		// kill
	pid= uiarg(0);
	sig= uiarg(2);
	result= kill(pid, sig);
	break;
    case 40:		// pipe
	addr= uiarg(0);
	if (addr==0) { result=-1; errno=EFAULT; break; }
	result= pipe(pipefd);
	if (result==-1) break;
	putui(addr, pipefd[0] & 0xffff);
	addr += 2;
	putui(addr, pipefd[1] & 0xffff);
	break;
    case 41:		// getgid
	break;
	result= getgid();
	break;
    case 44:		// geteuid
	result= geteuid();
	break;
    case 45:		// getegid
	result= getegid();
	break;
    case 48:		// fchdir
	fd= uiarg(0);
	result= fchdir(fd);
	break;
    case 50:		// fchown
	fd= uiarg(0);
 	owner= uiarg(2);
 	group= uiarg(4);
	result= fchown(fd, owner, group);
	break;
    case 51:		// mkdir
	path= (const char *)xlate_filename((char *)get_memptr(uiarg(0)));
	if (path==NULL) { result=-1; errno=EFAULT; break; }
	mode= uiarg(2);
	result= mkdir(path, mode);
	break;
    case 52:		// rmdir
	path= (const char *)xlate_filename((char *)get_memptr(uiarg(0)));
	if (path==NULL) { result=-1; errno=EFAULT; break; }
	result= rmdir(path);
	break;
    case 53:		// setpgrp
	result= setpgrp();
	break;
    case 55:		// waitpid
	pid= uiarg(0);
	iptr= (int16_t *)get_memptr(uiarg(2));
	if (iptr==NULL) { result=-1; errno=EFAULT; break; }
	options= uiarg(4);
	result= waitpid(pid, &wstatus, options);
	// Put the status into memory
	*iptr= htoemu16((int16_t)wstatus & 0xffff);
	break;
    case 60:		// flock
	fd= uiarg(0);
 	options= uiarg(2);
	result= flock(fd, options);
	break;
    case 61:		// getpgrp
	result= getpgrp();
	break;
    case 77:		// setpgid
	pid= uiarg(0);
	pgid= uiarg(2);
	result= setpgid(pid, pgid);
	break;
    case 78:		// setsid
	result= setsid();
	break;
    case 79:		// getsid
	pid= uiarg(0);
	result= getsid(pid);
	break;

    default: fprintf(stderr, "Unhandled syscall %d\n", op); exit(1);
  }

  sres= (int32_t)(result & 0xffff);
  return(sres);
}
