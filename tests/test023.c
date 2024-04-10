void cprintf(char *fmt, ...);

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void *ptr[16];

int main() {
  int num, amt;
  srand(0);

  while (1) {
    num= rand() & 0xf;
    amt= rand() & 0xff;
    cprintf("num is %d amt 0x%x\n", num, amt);

    if (ptr[num] != NULL) {
      cprintf("freeing ptr[%d]\n", num);
      free(ptr[num]);
      ptr[num]= NULL;
    }

    cprintf("Allocating 0x%x bytes to ptr[%d]\n", amt, num);
    ptr[num]= malloc(amt);
    cprintf("ptr[%d] is now 0x%x\n", num, ptr[num]);
  }
  return(0);
}
