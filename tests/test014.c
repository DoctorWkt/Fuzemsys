// waitpid() test

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main() {
  pid_t pid;
  int status, options=0;
  int i, j, k;

  pid= fork();
  if (pid==0) {
    cprintf("child after the fork\n");
    exit(0);
  }
  else if (pid==-1)
    cprintf("fork failed\n");
  else {
    pid= waitpid(pid, &status, options);
    cprintf("waitpid returned\n");
  }
  return(0);
}
