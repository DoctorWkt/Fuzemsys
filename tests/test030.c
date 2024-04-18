// open directory test

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

void cprintf(char *fmt, ...);

// FUZIX dirent structure
struct fdirent {
  unsigned int d_ino;
  char         d_name[30];
} D;


int main() {
  int fd, err;

  fd= open("in", O_RDONLY);
  if (fd==-1) {
    cprintf("open . failed\n"); return(1);
  }

  while (1) {
    err= read(fd, &D, sizeof(D));
    if (err<=0) break;
    // Can't print ino out as might be different as last time
    // cprintf("ino %d name %s\n", D.d_ino, D.d_name);
    cprintf("name %s\n", D.d_name);
  }

  close(fd);
  return(0);
}
