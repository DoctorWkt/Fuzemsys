// errno tests

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

char buf[512];

int main() {
  int err;

  err= read(300, buf, 1); cprintf("read err %d, errno %d\n", err, errno);
  err= close(300); cprintf("close err %d, errno %d\n", err, errno);
  err= chdir("floahen2k4"); cprintf("chdir err %d, errno %d\n", err, errno);
  err= mkdir(".", 0755); cprintf("mkdir err %d, errno %d\n", err, errno);
  err= rmdir("/"); cprintf("rmdir err %d, errno %d\n", err, errno);
  err= lseek(1, 0, SEEK_SET); cprintf("lseek err %d, errno %d\n", err, errno);
  err= setuid(0); cprintf("setuid err %d, errno %d\n", err, errno);

  return(0);
}
