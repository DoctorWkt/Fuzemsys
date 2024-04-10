// unsigned minus test

void cprintf(char *fmt, ...);

#include <stdio.h>

int main() {
  unsigned int x, y;

  x= 0xF000;
  cprintf("x starts at 0x%x\n", x);
  y= 0x1000; x -= y;
  cprintf("subtracting 0x%x gets 0x%x\n", y, x);
  y= 0x9000; x -= y;
  cprintf("subtracting 0x%x gets 0x%x\n", y, x);
  y= 0x9000; x -= y;
  cprintf("subtracting 0x%x gets 0x%x\n", y, x);
  return(0);
}
