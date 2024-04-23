/*
 *	Fake 6809 mini machine to run compiler tests
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <endian.h>
#include <sys/stat.h>

#include "d6809.h"
#include "e6809.h"
#include "exec.h"
#include "syscalls.h"
#include "mapfile.h"
#include "emumon.h"

// Now visible globally for syscalls.c
uint8_t ram[65536];

FILE *logfile=NULL;
char *mapfile = NULL;

// Default environment variables
static char *default_envp[] = {
  "PATH=/bin:/usr/bin:.",
  "HOME=/",
  "TERM=vt100",
  "USER=root",
  NULL
};

unsigned char e6809_read8(unsigned addr) {
  return ram[addr];
}

unsigned char e6809_read8_debug(unsigned addr) {
  if (addr < 0xFE00 || addr >= 0xFF00)
    return ram[addr];
  else
    return 0xFF;
}

static unsigned char fefcval = 0;

void e6809_write8(unsigned addr, unsigned char val) {
  int x;

  /* Writes to certain addresses act like system calls */
  /* 0xFEFF:  exit() with the val as the exit value */
  /* 0xFEFE:  putchar(val) */
  /* 0xFEFC/D: print out the 16-bit value as a decimal */

  switch (addr) {
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
    x = (fefcval << 8) | val;
    if (x > 0x8000)
      x -= 0x10000;
    printf("%d\n", x);
    break;
  case 0xFEFC:
    fefcval = val;		/* Save high byte for now */
    break;
  default:
    ram[addr] = val;
  }
}

/* FUZIX executable header */
static struct exec E;

/* Load an executable into memory */
/* and return the initial PC value */
static uint16_t load_executable(char *filename) {
  int fd;
  int cnt;
  int loadaddr;
  int bssend;
  struct stat S;

  /* Open the file */
  fd = open(filename, O_RDONLY);
  if (fd == -1) {
    perror(filename);
    exit(1);
  }

  /* Get the file's size */
  if (fstat(fd, &S) == -1) {
    perror(filename);
    exit(1);
  }

  /* Read in a possble FUZIX header */
  cnt = read(fd, &E, sizeof(E));
  if (cnt == sizeof(E)) {
    /* Check the magic number and CPU */
    if ((be16toh(E.a_magic) == EXEC_MAGIC) && (E.a_cpu == A_6809)) {

      /* Determine the load address. */
      /* N.B. Add on the entry size so we */
      /* don't have to lseek back to the start */
      loadaddr = (E.a_base << 8) + E.a_entry;

      /* Determine the first address after the BSS */
      bssend = (E.a_endhi << 8) + E.a_endlo + 1;
      set_initial_brk(bssend);
// printf("Set initial brk to 0x%x\n", bssend);

      /* Now read in the rest of the file */
      cnt = read(fd, &ram[loadaddr], 0xfff0);
      close(fd);
      return(loadaddr);
    }
  }

  /* It's not a FUZIX binary, so read it in as a raw file at addr 0 */
  if (read(fd, ram, 0xFC00) < 10) {
    fprintf(stderr, "emu6809: executable not FUZIX, too small.\n");
    perror(filename);
    exit(1);
  }
  close(fd);
  /* Set the starting address as 0x100 for a raw file */
  return(0x100);
}

#ifdef DEBUG
// Debug code
void dumpram() {
  FILE *out= fopen("memory.dump", "wb");
  fwrite(ram, 0x10000, 1, out);
  fclose(out);
}
#endif

void usage(char *name) {
  fprintf(stderr, "Usage: %s [-M] [-d logfile] [-m mapfile] [-b addr] executable <arguments>\n\n", name);
  fprintf(stderr, "\t-d: write debugging information to logfile\n");
  fprintf(stderr, "\t-m: load a mapfile with symbol information\n");
  fprintf(stderr, "\t-M: start in the monitor\n");
  fprintf(stderr, "\t-b: set breakpoint at address (decimal or $hex)\n\n");
  fprintf(stderr, "\tIf the FUZIXROOT environment variable is set,\n");
  fprintf(stderr, "\tuse that as the executable's root directory.\n");
  exit(1);
}

int set_arg_env(uint16_t sp, char **argv, char **envp);
void set_fuzix_root(char *dirname);

int main(int argc, char *argv[]) {
  int fd, pc, opt;
  uint16_t sp;
  char *fuzixroot;
  int start_in_monitor=0;
  int breakpoint;
  char **brkstr;		// Array of breakpoint strings
  int i, brkcnt=0;

  if (argc<2) usage(argv[0]);

  // Create an array to hold any breakpoint string pointers
  brkstr= (char **)malloc(argc * sizeof(char *));
  if (brkstr==NULL) exit(1);

  // Initialise the monitor before we try to set
  // up any command-line breakpoints
  monitor_init();

  while ((opt = getopt(argc, argv, "+d:m:Mb:")) != -1) {
    switch (opt) {
    case 'd':
      logfile= fopen(optarg, "w+");
      if (logfile==NULL) {
	fprintf(stderr, "Unable to open %s\n", optarg); exit(1);
      }
      break;
    case 'm':
      mapfile = optarg;
      read_mapfile(mapfile);
      break;
    case 'M':
      start_in_monitor=1;
      break;
    case 'b':
      // Cache the pointer for now
      brkstr[brkcnt++]= optarg;
      break;
    default:
      usage(argv[0]);
    }
  }

  // Clear the memory
  memset(ram, 0, 0x10000);

  // Load the executable file
  pc=load_executable(argv[optind]);

  // If we didn't load a map file, append
  // ".map" to the executable filename.
  // If that file exists, load that as a
  // map file.
  if (mapfile_loaded==0) {
    mapfile= (char *)malloc(strlen(argv[optind]) + 4);
    if (mapfile != NULL) {
      strcpy(mapfile, argv[optind]);
      strcat(mapfile, ".map");
      fd= open(mapfile, O_RDONLY);
      if (fd!=-1) {
	close(fd);
      	read_mapfile(mapfile);
      }
      free(mapfile);
    }
  }

  // Now that we might have a map file,
  // parse any breakpoint strings and set them
  for (i=0; i<brkcnt; i++) {
    breakpoint= parse_addr(brkstr[i], NULL);
    if (breakpoint != -1)
      set_breakpoint(breakpoint, BRK_INST);
  }

  // Put the args and envp on the stack.
  // Start the stack below the emulator special locations.
  sp= set_arg_env(0xFDFF, &argv[optind], default_envp);

  // If we have a FUZIXROOT environment variable,
  // use that as the executable's root directory.
  fuzixroot= getenv("FUZIXROOT");
  if (fuzixroot != NULL)
    set_fuzix_root(fuzixroot);
  else
    set_fuzix_root("");

  // Reset the CPU state
  e6809_reset(sp,pc);

  // Start in the monitor if needed
  if (start_in_monitor) {
    pc= monitor(pc);
    // Change the start address if the monitor says so
    if (pc!=-1)
      e6809_reset(sp,(uint16_t)pc);
  }

  // Otherwise loop executing instructions
  while (1)
    e6809_sstep(0, 0);
  return 0;
}
