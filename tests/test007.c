// Some lseek() tests

extern void ccprintchar(int);
extern void ccprintint(int);
void cprintf(char *fmt, ...);

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

char buf[16];

off_t mylseek(int fd, off_t pos, int whence)
{
  int e;
  off_t t = pos;
  // int *iptr= (int *)&t;

  // cprintf("mylseek t   %x %x\n", iptr[0], iptr[1]);
  // iptr= (int *)&pos;
  // cprintf("mylseek pos %x %x\n", iptr[0], iptr[1]);
  e = _lseek(fd, &t, whence);
  if (e == -1)
    return (off_t)e;
  return t;
}

int main() {
  int fd,err;

  fd= open("in/textfile1.txt", O_RDONLY);
  if (fd==-1) {
    cprintf("Cannot open in/textfile1.txt\n"); return(1);
  }

  err= (int)lseek(fd, 50, SEEK_SET);
  if (err==-1) {
    cprintf("Cannot lseek(fd, 50, SEEK_SET)\n"); return(1);
  }
  read(fd, buf, 16);
  write(1, buf, 16);

  err= (int)lseek(fd, 16, SEEK_CUR);
  if (err==-1) {
    cprintf("Cannot lseek(fd, 16, SEEK_CUR)\n"); return(1);
  }
  read(fd, buf, 16);
  write(1, buf, 16);

  err= (int)lseek(fd, -16, SEEK_END);
  if (err==-1) {
    cprintf("Cannot lseek(fd, -16, SEEK_END)\n"); return(1);
  }
  read(fd, buf, 16);
  write(1, buf, 16);

  close(fd);
  return(0);
}
