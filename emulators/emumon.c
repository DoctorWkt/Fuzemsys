// Monitor for the FUZIX emulators
// (c) 2024 Warren Toomey, GPL3
//
// Modelled on the monitor in the 6809 emulator by Arto Salmi

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <readline/readline.h>
#include "mapfile.h"

#ifdef CPU_6809
// Get 8-bit value in memory at the given location
int getub(int addr) {
  return(e6809_read8(addr));
}
#endif

#if 1
static unsigned char memory[65536];

int getub(int addr) {
  return(memory[addr]);
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
  { "rb", CMD_RBRK },
  { "rbrk",  CMD_RBRK },
  { "wb", CMD_WBRK },
  { "wbrk",  CMD_WBRK },
  { "nb", CMD_NOBRK },
  { "nbrk",  CMD_NOBRK },
  { NULL,  0 }
};

// Breakpoint types
enum brkpt_nums
{
  BRK_EMPTY,
  BRK_READ,
  BRK_WRITE,
  BRK_RDWR
};

// Associated strings
char *bpt_str[] = {
  NULL,
  "rd",
  "wr",
  "rw"
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
static void set_breakpoint(int addr, int type) {
  int i;

  for (i=0; i<NUM_BRKPOINTS; i++)
    if (brkpointlist[i].brktype== BRK_EMPTY) {
      brkpointlist[i].brktype= type;
      brkpointlist[i].addr= addr;
      return;
    }
  printf("No free breakpoint slot to set a breakpoint!\n");
}

// Dump memory
void dump_mem (int start, int end)
{
  int addr = start;
  int lsize;
  char abuf[17];

  if (start > end)
  {
    printf("addresses out of order\n");
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
      abuf[lsize] = isprint(mb)?mb:'.';
    }
    abuf[lsize] = '\0';
    while (lsize++ < 16) printf("   "); 
 
    puts(abuf);

    if(addr > end) break;
  }
}

// Given a string that represents an address,
// return the value of that address. Also set
// issym true if the string used a symbol.
// See the usage below for more details.
// -1 is returned if address is unparseable
static int parse_addr(char *addr, int *issym) {
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
  printf("s, step                   - single step\n");
  printf("x, exit                   - exit the monitor, back to running\n");
  printf("q, quit                   - quit the emulation\n");
  printf("g, go <addr>              - start execution at address\n");
  printf("p, print <addr> [<addr2>] - dump memory in the address range\n");
  printf("d, dis <addr> [<addr2>]   - disassemble memory in the address range\n");
  printf("w, write <addr> <value>   - overwrite memory with value\n");
  printf("b, brk [<addr>]           - set read/write breakpoint at <addr> or\n");
  printf("                            show list of breakpoints\n");
  printf("rb, rbrk <addr>           - set a read breakpoint at <addr>\n");
  printf("wb, wbrk <addr>           - set a write breakpoint at <addr>\n");
  printf("nb, nbrk [<addr>]         - remove breakpoint at <addr>, or all\n");

  printf("\n\nAddresses and Values\n\n");
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
int emumon(void) {
  char  *cmd_str;
  char  *arg[10];
  char *sym;
  int   arg_count;
  int   i, cmd, addr, addr2, offset;

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
	  if (addr!=-1) set_breakpoint(addr, BRK_RDWR);
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

      case CMD_RBRK:
	if (arg_count!=2) {
	  printf("  Usage: %s <addr>\n", arg[0]); break;
	}
	addr= parse_addr_msg(arg[1], NULL);
	if (addr!=-1) set_breakpoint(addr, BRK_READ);
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

      case CMD_PRINT:
	if (arg_count!=3) {
	  printf("  Usage: %s <addr> <addr2>\n", arg[0]); break;
	}
	addr= parse_addr_msg(arg[1], NULL);
	addr2= parse_addr_msg(arg[2], NULL);
	if (addr!= -1 || addr != -1)
	  dump_mem(addr, addr2);
	break;

      default: monitor_usage();
    }
  }
}

void main() {
  monitor_init();
  int addr;

  read_mapfile("test022.map");

  while (1) {
    addr= emumon();
    printf("Out of emumon with address %d\n", addr);
  }
}
