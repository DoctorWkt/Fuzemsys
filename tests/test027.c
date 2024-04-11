#include <stdio.h>
void cprintf(char *fmt, ...);

int main() {
  unsigned char *bstart= NULL;
  int len, cc, rv = 0;

  len= 12;
  cc= 12;
  cprintf("len %d cc %d\n", len, cc);

  bstart += cc; len -= cc;
  cprintf("len %d cc %d\n", len, cc);

  return(0);
}
