// stat() test

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

int main() {
  int fd, res;
  int size;
  struct stat S;

  fd= open("in/textfile1.txt", O_RDONLY);
  if (fd==-1) {
    cprintf("open on in/textfile1.txt failed: %d\n", errno); exit(1);
  }

  res= fstat(fd, &S);
  if (res==-1) {
    cprintf("fstat on in/textfile1.txt failed: %d\n", errno); exit(1);
  }
  size= S.st_size & 0xffff;
  cprintf("in/textfile1.txt is %d bytes\n", size);
  close(fd);
  return(0);
}
