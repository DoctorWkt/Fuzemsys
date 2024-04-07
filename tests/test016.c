// stat() test

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
  int res;
  int size;
  struct stat S;
  res= stat("in/textfile1.txt", &S);
  if (res==-1) {
    cprintf("stat on in/textfile1.txt failed: %d\n", errno); exit(1);
  }
  size= S.st_size & 0xffff;
  cprintf("in/textfile1.txt is %d bytes\n", size);
  return(0);
}
