#ifndef EMUMON_H
# define EMUMON_H

/* Breakpoint types */
enum brkpt_nums
{
  BRK_EMPTY,
  BRK_WRITE,
  BRK_INST
};

/* emumon.c */
void set_breakpoint(int addr, int type);
int is_breakpoint(int addr, int type);
int parse_addr(char *addr, int *issym);
void monitor_init(void);
int monitor(int addr);

#endif
