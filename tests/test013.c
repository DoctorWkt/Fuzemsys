// fork() test

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <stdio.h>
#include <unistd.h>

int main() {
  int pid;

  pid= fork();
  if (pid==0)
    cprintf("child after the fork\n");
  else if (pid==-1)
    cprintf("fork failed\n");
  else
    cprintf("parent after the fork\n");
  return(0);
}
