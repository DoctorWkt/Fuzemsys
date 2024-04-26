/*
 *	Fake mini machine to run compiler tests
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "libz80/z80.h"
#include "z80dis.h"
#include "exec.h"
#include "syscalls.h"
#include "mapfile.h"

// Now visible globally for syscalls.c
uint8_t ram[65536];
FILE *logfile=NULL;
char *mapfile = NULL;

Z80Context cpu_z80;
static unsigned trace;

// Default environment variables
static char *default_envp[] = {
  "PATH=/bin:/usr/bin:.",
  "HOME=/",
  "TERM=vt100",
  "USER=root",
  NULL
};

uint8_t mem_read(int unused, uint16_t addr)
{
    return ram[addr];
}

void mem_write(int unused, uint16_t addr, uint8_t val)
{
    ram[addr] = val;
}

uint8_t io_read(int unused, uint16_t port)
{
    return 0xFF;
}

/* Writes to certain ports act like system calls */
/* 0xFD:    print out the 8-bit value as a char if printable, else hex */
/* 0xFF:    exit() with the val as the exit value */
/* 0xFE:    putchar(val) */
/* 0xFB/FC: print out the 16-bit value as a decimal */

static unsigned char fcval = 0;
void io_write(int unused, uint16_t port, uint8_t value)
{
    int x;

    switch(port & 0xFF) {
    case 0xFD:
        if (value < 32 || value > 127)
            printf("\\x%02X", value);
        else
            putchar(value);
        fflush(stdout);
        return;
    case 0xFF:
        if (value)
            fprintf(stderr, "***FAIL %d\n", value);
        exit(value);
    case 0xFE:
    	putchar(value);
    	break;
    case 0xFC:
    	/* Make the value signed */
    	x = (fcval << 8) | value;
    	if (x > 0x8000)
      	    x -= 0x10000;
    	printf("%d\n", x);
    	break;
    case 0xFB:
    	fcval = value;              /* Save high byte for now */
    	break;
    default:
        fprintf(stderr, "***BAD PORT %d\n", port);
        exit(1);
    }
}

static unsigned int nbytes;

uint8_t z80dis_byte(uint16_t addr)
{
	uint8_t r = mem_read(0, addr);
	fprintf(stderr, "%02X ", r);
	nbytes++;
	return r;
}

uint8_t z80dis_byte_quiet(uint16_t addr)
{
	return mem_read(0, addr);
}

static void z80_trace(unsigned unused)
{
	static uint32_t lastpc = -1;
	char buf[256];

	if (!trace)
		return;
	nbytes = 0;
	/* Spot XXXR repeating instructions and squash the trace */
	if (cpu_z80.M1PC == lastpc && z80dis_byte_quiet(lastpc) == 0xED &&
		(z80dis_byte_quiet(lastpc + 1) & 0xF4) == 0xB0) {
		return;
	}
	lastpc = cpu_z80.M1PC;
	fprintf(stderr, "%04X: ", lastpc);
	z80_disasm(buf, lastpc);
	while(nbytes++ < 6)
		fprintf(stderr, "   ");
	fprintf(stderr, "%-16s ", buf);
	fprintf(stderr, "[ %02X:%02X %04X %04X %04X %04X %04X %04X ]\n",
		cpu_z80.R1.br.A, cpu_z80.R1.br.F,
		cpu_z80.R1.wr.BC, cpu_z80.R1.wr.DE, cpu_z80.R1.wr.HL,
		cpu_z80.R1.wr.IX, cpu_z80.R1.wr.IY, cpu_z80.R1.wr.SP);
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


int main(int argc, char *argv[])
{
  int fd, pc, sp, opt;
  char *fuzixroot;
  char **brkstr;                // Array of breakpoint strings
  int brkcnt=0;

  if (argc<2) usage(argv[0]);

  // Create an array to hold any breakpoint string pointers
  brkstr= (char **)malloc(argc * sizeof(char *));
  if (brkstr==NULL) exit(1);

  // Initialise the monitor before we try to set
  // up any command-line breakpoints
  // monitor_init();

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
    // case 'M':
      // start_in_monitor=1;
      // break;
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
  // for (i=0; i<brkcnt; i++) {
    // breakpoint= parse_addr(brkstr[i], NULL);
    // if (breakpoint != -1)
      // set_breakpoint(breakpoint, BRK_INST);
  // }

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
  Z80RESET(&cpu_z80);
  cpu_z80.ioRead = io_read;
  cpu_z80.ioWrite = io_write;
  cpu_z80.memRead = mem_read;
  cpu_z80.memWrite = mem_write;
  cpu_z80.trace = z80_trace;

  while(1)
    Z80ExecuteTStates(&cpu_z80, 1000);
}