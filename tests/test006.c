// access test

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
  int err;

  err= access("Makefile", F_OK);
  if (err==-1) { cprintf("Unable to access Makefile\n"); return(1); }
  err= access("foo", F_OK);
  if (err==-1) { cprintf("Unable to access foo\n"); return(0); }
  return(0);
}
