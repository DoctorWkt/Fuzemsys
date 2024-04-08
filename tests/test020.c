// kill() test

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <unistd.h>
#include <signal.h>

int main() {
  int pid;
  int err;

  cprintf("About to do kill() on myself\n");
  pid= getpid();
  err= kill(pid, SIGKILL);
  if (err == -1) {
    cprintf("kill() failed\n"); return(1);
  }

  cprintf("This should never get printed out\n");

  return(0);
}
