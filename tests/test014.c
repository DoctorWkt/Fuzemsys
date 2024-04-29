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
    // Loop to delay the process
    for (i=0; i < 256; i++) {
      for (j=0; j < 16; j++) {
	k= i * j; cprintf("%d ", k);
      }
      cprintf("\n");
    }
    exit(0);
  }
  else if (pid==-1)
    cprintf("fork failed\n");
  else {
    cprintf("parent after the fork\n");
    pid= waitpid(pid, &status, options);
    cprintf("waitpid returned\n");
  }
  return(0);
}
