// brk test

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <stdio.h>
#include <unistd.h>

int main() {
  int addr;
  int res;

  for (addr=0; addr <= 0xfe00; addr += 0x100) {
    res= brk((void *)addr);
    cprintf("brk on %x gives %d\n", addr, res);
  }
  return(0);
}
