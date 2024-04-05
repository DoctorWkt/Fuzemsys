// brk test

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <stdio.h>
#include <unistd.h>

int main() {
  int i;
  unsigned int res;

  for (i=0; i < 256; i++) {
    res= (unsigned int)sbrk(0x100);
    cprintf("sbrk try %d gives %x\n", i, res);
  }
  return(0);
}
