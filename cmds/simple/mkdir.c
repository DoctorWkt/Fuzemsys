#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cprintf(char *fmt, ...);

int main(int argc, char *argv[]) {
  int i;

  if (argc < 2) {
    cprintf("Usage: mkdir files...\n");
    exit(1);
  }

  for (i = 1; i < argc; i++) {
    if (mkdir(argv[i], 0755) < 0) {
      cprintf("mkdir: %s failed to create\n", argv[i]);
      break;
    }
  }

  exit(0); return(0);
}
