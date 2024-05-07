// ftruncate test

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

char buf[512];

int main() {
  int fd, cnt;

  unlink("tmpfile");

  fd= open("tmpfile", O_CREAT|O_WRONLY, 0644);
  if (fd==-1) {
    printf("Cannot write tmpfile\n"); return(1);
  }

  cnt= write(fd, "Hello world\n", 12);
  cnt= write(fd, "Hello world\n", 12);
  cnt= write(fd, "Hello world\n", 12);
  cnt= write(fd, "Hello world\n", 12);
  cnt= write(fd, "Hello world\n", 12);
  cnt= write(fd, "Hello world\n", 12);
  cnt= write(fd, "Hello world\n", 12);
  cnt= write(fd, "Hello world\n", 12);
  cnt= write(fd, "Hello world\n", 12);
  cnt= write(fd, "Hello world\n", 12);
  close(fd);

  fd= open("tmpfile", O_RDWR);
  if (fd==-1) {
    printf("Cannot reopen tmpfile\n"); return(1);
  }

  cnt= ftruncate(fd, (long)36);
  printf("After ftruncate, cnt %d\n", cnt);

  fd= open("tmpfile", O_RDONLY);
  if (fd==-1) {
    printf("Cannot open tmpfile\n"); return(1);
  }

  while (1) {
    cnt= read(fd, buf, 512);
    if (cnt <=0) break;
    write(1, buf, cnt);
  }

  if (close(fd)== -1) {
    printf("Cannot close open fd\n"); return(1);
  }
  unlink("tmpfile");

  return(0);
}
