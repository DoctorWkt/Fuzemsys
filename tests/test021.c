// flock() test

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>

int main() {
  int fd, err;

  fd= open("in/textfile1.txt", O_RDONLY);
  if (fd==-1) {
    cprintf("Cannot open in/textfile1.txt\n"); return(1);
  }

  err= flock(fd, LOCK_EX);
  if (err==-1) {
    cprintf("flock() on in/textfile1.txt failed\n"); return(1);
  }
  cprintf("flock(\"in/textfile1.txt\", LOCK_EX) OK\n");

  return(0);
}
