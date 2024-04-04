// Link a file, rename it, unlink it

extern void cprintchar(int);
extern void cprintint(int);
void cprintf(char *fmt, ...);

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
  int err;

  err= link("in/textfile1.txt", "tmpfile");
  if (err==-1) {
    cprintf("Cannot link in/textfile1.txt to tmpfile\n"); return(1);
  }

  err= rename("tmpfile", "tmpfile2");
  if (err==-1) {
    cprintf("Cannot rename tmpfile to tmpfile2\n"); return(1);
  }

  err= unlink("tmpfile2");
  if (err==-1) {
    cprintf("Cannot unlink tmpfile2\n"); return(1);
  }

  return(0);
}
