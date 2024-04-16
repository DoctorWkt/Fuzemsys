// Monitor for the FUZIX emulators
// (c) 2024 Warren Toomey, GPL3
//
// Modelled on the monitor in the 6809 emulator by Arto Salmi.
//
// If you are porting to another emulator, scroll
// down and read the PORTING guide.

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <readline/readline.h>
#include "emumon.h"
#include "mapfile.h"

// PORTING NOTES

// Firstly, the monitor uses readline() which has a global variable
// called PC. If your emulator also has a global variable called PC,
// you will need to rename it or readline() will tromp all over it.
//
// You need to define these "shim" functions in your emulator so that
// the monitor can access memory etc.
// - static uint8_t getub(int addr)
// - static int disassemble_instruction(char *buf, int addr)
// - static void get_cpu_state(char *buf)
// - static int run_instruction(void)
// - static void write_mem(int addr, int val)
//
// See the comments above these functions in the CPU_6809 area below.
// Note that addresses are not constrained to be 16-bits only, so you
// will need to truncate them if necessary.
//
// The monitor itself provides these functions:
// - void set_breakpoint(int addr, int type)
// - int is_breakpoint(int addr, int type)
// - int parse_addr(char *addr, int *issym)
// - void monitor_init(void)
// - int monitor(int addr)
//
// The breakpoint types are defined in emumon.h. monitor_init()
// should be called before any breakpoints are set.
//
// parse_addr() calls get_sym_address() from mapfile.c. If you
// use it to parse addresses then run read_mapfile() first.
// Have a look at the main() code in emu6809.c as an example.
//
// monitor() needs the current PC as argument. It returns -1 to be
// ignored, or the address of the next instruction to execute.
//
// To deal with breakpoints in your emulator, set a global variable
// after a memory location is written to. Then, in your main loop,
// have some code like this (from e6809.c):
//        // If the PC is a breakpoint, or we hit a write
//        // breakpoint, fall into the monitor
//        if (write_brkpt==1 || is_breakpoint(reg_pc, BRK_INST)) {
//          write_brkpt=0;
//          addr= monitor(reg_pc);
//
//          // If we have a new PC from the monitor, set it
//          if (addr != -1)
//            reg_pc= addr & 0xffff;
//        }


#ifdef CPU_6809
#include "e6809.h"
#include "d6809.h"

// Get 8-bit value in memory at the given location
static uint8_t getub(int addr) {
  return(e6809_read8(addr));
}

// Fill a buffer with the disassembly of the
// instruction at the given address. Return
// the address of the next instruction. The
// buffer is 80 characters long.
static int disassemble_instruction(char *buf, int addr) {
  addr+= d6809_disassemble(buf, addr);
  return(addr);
}

// Fill a buffer with the current CPU state.
// The buffer is 80 characters long.
static void get_cpu_state(char *buf) {
  e6809_get_statestr(buf);
}

// Execute one instruction and get the
// address of the following instruction.
static int run_instruction(void) {
  return(e6809_sstep(0, 0));
}

// Write the given value to memory at
// the given address. We must deal with
// oversized arguments.
static void write_mem(int addr, int val) {
  e6809_write8(addr, val & 0xff);
}
#endif

// List of commands
enum cmd_numbers
{
  CMD_BRK,
  CMD_DISASM,
  CMD_EXIT,
  CMD_GO,
  CMD_NOBRK,
  CMD_PRINT,
  CMD_QUIT,
  CMD_RBRK,
  CMD_STEP,
  CMD_WBRK,
  CMD_WRITE
};

// Commands and matching words
typedef struct
{
  char *cmd_str;
  int   cmd_num;
} command;

static command cmd_table[] = {
  { "s", CMD_STEP },
  { "step", CMD_STEP },
  { "x", CMD_EXIT },
  { "exit",  CMD_EXIT },
  { "q", CMD_QUIT },
  { "quit",  CMD_QUIT },
  { "g", CMD_GO },
  { "go",  CMD_GO },
  { "p", CMD_PRINT },
  { "print",  CMD_PRINT },
  { "d", CMD_DISASM },
  { "dis",  CMD_DISASM },
  { "w", CMD_WRITE },
  { "write",  CMD_WRITE },
  { "b", CMD_BRK },
  { "brk",  CMD_BRK },
  { "wb", CMD_WBRK },
  { "wbrk",  CMD_WBRK },
  { "nb", CMD_NOBRK },
  { "nbrk",  CMD_NOBRK },
  { NULL,  0 }
};

// Breakpoint strings
char *bpt_str[] = {
  NULL,
  "wr",
  "pc"
};

// List of breakpoint addresses
typedef struct {
  unsigned int addr;
  int brktype;
} brkpoint;

#define NUM_BRKPOINTS 30
static brkpoint brkpointlist[NUM_BRKPOINTS];

// Remove a breakpoint at the given address
static void remove_breakpoint(int addr) {
  int i;
  for (i=0; i<NUM_BRKPOINTS; i++) { 
    if (brkpointlist[i].addr == addr)
      brkpointlist[i].brktype= BRK_EMPTY;
  }
}

// Remove all breakpoints
static void remove_all_breakpoints(void) {
  int i;
  for (i=0; i<NUM_BRKPOINTS; i++)
    brkpointlist[i].brktype= BRK_EMPTY;
}

// Set a breakpoint
void set_breakpoint(int addr, int type) {
  int i;

  for (i=0; i<NUM_BRKPOINTS; i++)
    if (brkpointlist[i].brktype== BRK_EMPTY) {
      brkpointlist[i].brktype= type;
      brkpointlist[i].addr= addr;
      return;
    }
  printf("No free breakpoint slot to set a breakpoint!\n");
}

// This is the address of a breakpoint we can
// ignore. We use this when single-stepping.
// It gets ignored for one is_breakpoint() call.
static int ignored_addr= -1;

static void ignore_breakpoint(int addr) {
  ignored_addr= addr;
}

// Return 1 if there is a breakpoint of the given
// type at the given address, 0 otherwise.
int is_breakpoint(int addr, int type) {
  int i;

  // Deal with the ignored breakpoint
  if (addr==ignored_addr) {
    ignored_addr= -1;
    return(0);
  }

  for (i=0; i<NUM_BRKPOINTS; i++)
    if (brkpointlist[i].brktype== type && 
	brkpointlist[i].addr == addr)
      return(1);
  return(0);
}

// Dump or disassemble memory
static void dump_mem (int start, int end, int cmd)
{
  int addr = start;
  int newaddr;
  int lsize;
  char buf[80];
  char *sym=NULL;
  int offset;

  if (start > end)
  {
    printf("addresses out of order\n");
    return;
  }

  if (cmd==CMD_DISASM) {
    while(addr < end) {
      newaddr= disassemble_instruction(buf, addr);

      // See if we have a symbol at this address
      if (mapfile_loaded)
        sym= get_symbol_and_offset(addr, &offset);

      if (sym!=NULL)
        printf("%12s+%04X: %-16.16s\n", sym, offset, buf);
      else
        printf("%04X: %-16.16s\n", addr, buf);
      addr= newaddr;
    }
    printf("\n");
    return;
  }

  printf("$%04X to $%04X:\n",start,end);

  while(1)
  {
    printf("%04X: ",addr);

    for(lsize = 0;(addr < (end + 1)) && (lsize < 16);addr++,lsize++)
    {
      int mb = getub(addr);
      printf("%02X ",mb);
      buf[lsize] = isprint(mb) ? mb : '.';
    }
    buf[lsize] = '\0';
    while (lsize++ < 16) printf("   "); 
 
    printf("  "); puts(buf);

    if(addr > end) break;
  }
}

// Execute the given number of instructions
// starting at the given address.
// Disassemble each instruction beforehand,
// and print the CPU state out afterward.
// Return the address of the next instruction.
static int run_instructions(int cnt, int addr) {
  int i, offset;
  char buf[80];
  char *sym=NULL;

  for (i=0; i< cnt; i++) {
    disassemble_instruction(buf, addr);

    // See if we have a symbol at this address
    if (mapfile_loaded)
      sym= get_symbol_and_offset(addr, &offset);

    if (sym!=NULL)
      printf("%12s+%04X: %-16.16s | ", sym, offset, buf);
    else
      printf("%04X: %-16.16s | ", addr, buf);

    // Before we run the instruction, ignore any
    // breakpoint at that address, so we won't
    // fall back into the monitor :-)
    ignore_breakpoint(addr);
    addr=run_instruction();

    get_cpu_state(buf);
    printf("%s\n", buf);
  }
  return(addr);
}

// Given a string that represents an address,
// return the value of that address. Also set
// issym true if the string used a symbol.
// See the usage below for more details.
// -1 is returned if address is unparseable
int parse_addr(char *addr, int *issym) {
  char *offptr;
  int offset=0;
  int symaddr;

  // Error check
  if (addr == NULL || *addr == '\0') return(-1);

  // Assume no symbols
  if (issym != NULL) *issym=0;

  // Parse hexadecimal literals
  if (*addr == '$')
   return( (int) strtol(addr+1, NULL, 16));

  // Parse decimal literals
  if (isdigit(*addr))
   return( (int) strtol(addr, NULL, 10));

  // Find any offset and parse it
  offptr= strchr(addr, '+');
  if (offptr != NULL) {
    *offptr='\0';
    offset= parse_addr(offptr+1, NULL);
    // Offset was garbage
    if (offset==-1) return(-1);
  }

  // We now have a symbol
  if (issym != NULL) *issym=1;

  // Get the symbol's address
  symaddr= get_sym_address(addr);
  if (symaddr==-1) return(-1);
  return(symaddr+offset);
}

// As above but prints out an error message
static int parse_addr_msg(char *addr, int *issym) {
  int result= parse_addr(addr, issym);
  if (result==-1)
   printf("Address %s is malformed\n", addr);
  return(result);
}

// Break a command line into a number of arguments.
// Return the arguments in table and return the arg count.
static int str_scan (char *str, char *table[], int maxi)
{
  int i = 0;

  while(1)
  {
    while(isgraph(*str) == 0 && *str != '\0') str++;
    if (*str == '\0') return i;
    table[i] = str;
    if (maxi-- == 0) return i;

    while(isgraph(*str) != 0) str++;
    if (*str == '\0') return i;
    *str++ = '\0';
    i++;
  }
}

// Given a command string, return the
// matching command number or -1 if none.
static int get_command (char *str)
{
  int i;

  if (str == NULL || *str == '\0') return( -1);

  for (i=0; str[i]; i++)
    str[i]= tolower(str[i]);

  for (i = 0; cmd_table[i].cmd_str != NULL; i++)
    if (!strcmp(str,cmd_table[i].cmd_str))
      return(cmd_table[i].cmd_num);

  return(-1);
}

static void monitor_usage() {

  printf("Monitor usage:\n\n");
  printf("s, step <num>             - execute 1 or <num> instructions\n");
  printf("x, exit                   - exit the monitor, back to running\n");
  printf("q, quit                   - quit the emulation\n");
  printf("g, go <addr>              - start execution at address\n");
  printf("p, print <addr> [<addr2>] - dump memory in the address range\n");
  printf("d, dis <addr> [<addr2>]   - disassemble memory in the address range\n");
  printf("w, write <addr> <value>   - overwrite memory with value\n");
  printf("b, brk [<addr>]           - set instruction breakpoint at <addr> or\n");
  printf("                            show list of breakpoints\n");
  printf("wb, wbrk <addr>           - set a write breakpoint at <addr>\n");
  printf("nb, nbrk [<addr>]         - remove breakpoint at <addr>, or all\n\n");

  printf("Addresses and Values\n\n");
  printf("Decimal literals start with [0-9], e.g. 23\n");
  printf("Hexadecimal literals start with $, e.g. $1234\n");
  printf("Symbols start with _ or [A-Za-z], e.g. _printf\n");
  printf("Synbols + offset, e.g. _printf+23, _printf+$100\n");
}

// Initialise the monitor variables
void monitor_init (void) {
  remove_all_breakpoints();
}


// Monitor function: prompt user for commands and execute them.
// Returns either an address to start execution at, or -1 to
// continue execution at the current program counter.
int monitor(int addr) {
  char *cmd_str;
  char *arg[10];
  char *sym;
  int  arg_count;
  int  i, cmd, addr2, offset;
  int  issym, count, val;

  if (is_breakpoint(addr, BRK_INST))
    printf("Stopped at $%04X\n", addr);

  while (1) {
    fflush(stdout);
    fflush(stdin);
   
    cmd_str = readline("monitor>");

    if (*cmd_str == '\0') return(-1);
    arg_count = str_scan(cmd_str,arg,5) + 1;

#if 0
    // Debug
    for (cmd=0; cmd < arg_count; cmd++)
      printf("arg %d is %s\n", cmd, arg[cmd]);
#endif

    cmd= get_command(arg[0]);
    switch (cmd) {
      case CMD_QUIT: exit(0);
      case CMD_EXIT: return(-1);

      case CMD_BRK:
	if (arg_count==2) {
	  addr= parse_addr_msg(arg[1], NULL);
	  if (addr!=-1) set_breakpoint(addr, BRK_INST);
	} else {
	  // Otherwise print out the breakpoints
	  printf("Breakpoints: \n\n");
	  for (i=0; i<NUM_BRKPOINTS; i++)
	    if (brkpointlist[i].brktype != BRK_EMPTY) {
	      printf("  $%04X (%05d): %s",
		  brkpointlist[i].addr,
		  brkpointlist[i].addr,
		  bpt_str[ brkpointlist[i].brktype ]);
	      if (mapfile_loaded) {
		sym= get_symbol_and_offset(brkpointlist[i].addr, &offset);
		if (sym!=NULL)
		  printf("\t%s+%d", sym, offset);
	      }
	      printf("\n");
	    }
	}
	break;

      case CMD_WBRK:
	if (arg_count!=2) {
	  printf("  Usage: %s <addr>\n", arg[0]); break;
	}
	addr= parse_addr_msg(arg[1], NULL);
	if (addr!=-1) set_breakpoint(addr, BRK_WRITE);
	break;

      case CMD_NOBRK:
	if (arg_count==2) {
	  addr= parse_addr_msg(arg[1], NULL);
	  if (addr!=-1) remove_breakpoint(addr);
	} else {
	  remove_all_breakpoints();
	}
	break;

      case CMD_GO:
	if (arg_count!=2) {
	  printf("  Usage: %s <addr>\n", arg[0]); break;
	}
	addr= parse_addr_msg(arg[1], NULL);
	if (addr!=-1) return(addr);
	break;

      case CMD_DISASM:
      case CMD_PRINT:
	if (arg_count<2 || arg_count>3) {
	  printf("  Usage: %s <addr> [<addr2>]\n", arg[0]); break;
	}
	addr= parse_addr_msg(arg[1], &issym);
	// We have two addresses, so parse the second
	if (arg_count==3)
	  addr2= parse_addr_msg(arg[2], NULL);
	else {
	  // No second address. If the first address wasn't a symbol,
	  // add 0xFF to the first so we print 256 bytes.
	  // If a symbol, find the end address for the symbol
	  // (assuming it's a routine).
	  if (issym==0)
	    addr2= addr + 0xFF;
	  else
	    addr2= get_sym_end_address(arg[1]);
	}

	if (addr!= -1 || addr != -1)
	  dump_mem(addr, addr2, cmd);
	break;

      case CMD_STEP:
	if (arg_count>2) {
	  printf("  Usage: %s [<num>]\n", arg[0]); break;
	}
 	count=1;
	if (arg_count==2)
	  count= strtol(arg[1], NULL, 10);
	addr= run_instructions(count, addr);
	break;

	case CMD_WRITE:
	if (arg_count!=3) {
	  printf("  Usage: %s <addr> <value>\n", arg[0]); break;
	}
	addr= parse_addr_msg(arg[1], NULL);
	if (*arg[2]=='$')
	  val= strtol(arg[2]+1, NULL, 16);
	else
	  val= strtol(arg[2], NULL, 10);
	write_mem(addr, val);
	break;

      default: monitor_usage();
    }
  }
}
