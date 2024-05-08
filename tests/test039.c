#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

void strmode(mode_t mode, char *p) {
  *p++ = ' ';
  *p = '\0';
}

int main() {
  char buf[25];
  strmode((mode_t) 0100444, buf);
  printf("buf is %s\n", buf);
  return (0);
}
