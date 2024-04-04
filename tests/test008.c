// dup() test

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <unistd.h>
#include <fcntl.h>

char buf[512];

int main() {
  int fd, fd2, cnt;

  fd2= open("in/textfile1.txt", O_RDONLY);
  if (fd2==-1) {
    cprintf("Cannot open in/textfile1.txt\n"); return(1);
  }

  fd= dup(fd2);
  if (fd==-1) {
    cprintf("Unable to dup(fd2)\n"); return(1);
  }

  while (1) {
    cnt= read(fd, buf, 512);
    if (cnt <=0) break;
    write(1, buf, cnt);
  }

  if (close(fd)== -1) {
    cprintf("Cannot close open fd\n"); return(1);
  }

  return(0);
}
