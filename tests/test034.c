// sleep() test
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void cprintf(char *fmt, ...);

int main() {
  int x;
  x= sleep(2);
  cprintf("After sleep(), %d seconds remaining\n", x);
  return (0);
}
