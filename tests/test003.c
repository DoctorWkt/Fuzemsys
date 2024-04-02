// Create a file, close it, open it, print its contents

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <unistd.h>
#include <fcntl.h>

char buf[512];

int main() {
  int fd, cnt;

  fd= open("tmpfile", O_CREAT|O_WRONLY, 0644);
  if (fd==-1) {
    cprintf("Cannot write tmpfile\n"); return(1);
  }

  cnt= write(fd, "Hello world\n", 12);
  if (cnt!=12) {
    cprintf("Write tmpfile failed\n"); return(1);
  }

  if (close(fd)== -1) {
    cprintf("Cannot close open fd\n"); return(1);
  }

  fd= open("tmpfile", O_RDONLY);
  if (fd==-1) {
    cprintf("Cannot read tmpfile\n"); unlink("tmpfile"); return(1);
  }

  while (1) {
    cnt= read(fd, buf, 512);
    if (cnt <=0) break;
    write(1, buf, cnt);
  }

  if (close(fd)== -1) {
    cprintf("Cannot close open fd\n"); return(1);
  }

  unlink("tmpfile"); return(0);
}
