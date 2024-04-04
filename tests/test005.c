// Make a dir, cd into it, cd out, rmdir

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
  int err;

  err= mkdir("foo", 0777);
  if (err==-1) { cprintf("Unable to mkdir foo\n"); return(1); }
  err= chdir("foo");
  if (err==-1) { cprintf("Unable to chdir foo\n"); return(1); }
  err= chdir("..");
  if (err==-1) { cprintf("Unable to chdir ..\n"); return(1); }
  err= rmdir("foo");
  if (err==-1) { cprintf("Unable to rmdir foo\n"); return(1); }
  return(0);
}
