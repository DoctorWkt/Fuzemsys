// execve test

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <stdio.h>
#include <unistd.h>

char *argv[]= { "/bin/echo", "hello", "there", "how", "are", "you?", NULL };

int main() {
  int i, res;
  cprintf("About to exec %s with these arguments:\n", argv[0]);
  for (i=1; argv[i] != NULL; i++)
    cprintf("  %s\n", argv[i]);

  res= execve(argv[0], argv, NULL);
  cprintf("exec failed with result %d\n", res);
  return(0);
}
