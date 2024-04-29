// argv and envp tests

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <stdio.h>

int main(int argc, char **argv, char **envp) {
  int i;

  // cprintf("argc is %d at %x\n", argc, &argc);
  // cprintf("argv is at %x %x\n", argv, &argv);
  // cprintf("envp is at %x %x\n", envp, &envp);
  for (i=0; argv[i]!=NULL; i++)
    cprintf("argv[%d] is %s\n", i, argv[i]);
  for (i=0; envp[i]!=NULL; i++)
    cprintf("envp[%d] is %s\n", i, envp[i]);
  return(0);
}
