#include <stdio.h>

int main() {
  static char gt[30];
  char *p;
  p= gt;

  *p++ = 'W';
  *p++ = 'K';
  *p++ = 'T';

  printf("p-gt is %d %x\n", (p-gt), (p-gt));
  return(0);
}
