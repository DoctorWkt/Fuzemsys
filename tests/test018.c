// dup2() test

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <unistd.h>
#include <fcntl.h>

char buf[512];

int main() {
  int fd, fd2, cnt;
  int err;

  fd= open("in/textfile1.txt", O_RDONLY);
  if (fd==-1) {
    cprintf("Cannot open in/textfile1.txt\n"); return(1);
  }

  fd2=10;
  err= dup2(fd, fd2);
  if (err==-1) {
    cprintf("Cannot dup2(%d,%d)\n", fd, fd2); return(1);
  }
  
  while (1) {
    cnt= read(fd2, buf, 512);
    if (cnt <=0) break;
    write(1, buf, cnt);
  }

  if (close(fd)== -1) {
    cprintf("Cannot close open fd\n"); return(1);
  }
  if (close(fd2)== -1) {
    cprintf("Cannot close open fd2\n"); return(1);
  }

  return(0);
}
