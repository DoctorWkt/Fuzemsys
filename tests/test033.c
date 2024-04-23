// atoi() test
#include <stdio.h>
#include <stdlib.h>

void cprintf(char *fmt, ...);

int main() {
  int x;
  x= 12; cprintf("%d\n", x);
  x= atoi("12"); cprintf("%d\n", x);
  x= atoi("-12"); cprintf("%d\n", x);
  x= atoi("3000"); cprintf("%d\n", x);
  x= atoi("-3000"); cprintf("%d\n", x);
  x= atoi("32767"); cprintf("%d\n", x);
  x= atoi("-32767"); cprintf("%d\n", x);
  x= atoi("-32768"); cprintf("%d\n", x);
  return (0);
}
