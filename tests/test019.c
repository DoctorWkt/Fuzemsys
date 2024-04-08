// pipe() test

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <unistd.h>
#include <fcntl.h>

char buf[512];

int main() {
  int fd, cnt;
  int err;
  int pid;
  int pipefd[2];

  err= pipe(pipefd);
  if (err==-1) {
    cprintf("pipe() failed\n"); return(1);
  }

  // cprintf("pipe fds are %d and %d\n", pipefd[0], pipefd[1]);

  pid= fork();
  if (pid==-1) {
    cprintf("fork failed\n"); return(1);
  }
  if (pid==0) {
    close(pipefd[0]);
    fd= open("in/textfile1.txt", O_RDONLY);
    if (fd==-1) {
      cprintf("Cannot open in/textfile1.txt\n"); return(1);
    }
    while (1) {
      cnt= read(fd, buf, 512);
      if (cnt <=0) break;
      write(pipefd[1], buf, cnt);
    }
  } else {
    close(pipefd[1]);
    while (1) {
      cnt= read(pipefd[0], buf, 512);
      if (cnt <=0) break;
      write(1, buf, cnt);
    }
  }

  return(0);
}
