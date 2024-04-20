#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <alloc.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

// Tring to get readdir to work.
// My own version here.

static struct dirent buf;

struct dirent *rddir(DIR * dir) {
  struct __dirent *direntry;
  int cnt;

  if (dir == NULL) { errno = EFAULT; return NULL; }

  cnt= read(dir->dd_fd, dir->_priv.buf, sizeof(struct __dirent));
  if (cnt<=0) return(NULL);

  direntry= (struct __dirent *)dir->_priv.buf;

  buf.d_ino = direntry->d_ino;
  strncpy(buf.d_name, (char *) direntry->d_name, 31);
  buf.d_name[30] = 0;
  return &buf;
}

int main(int argc, char *argv[]) {
  struct dirent *Dirent;
  DIR *Dir;

  // Ensure we can open directory.
  Dir = opendir("in");
  if (Dir == NULL) {
    printf("Cannot open directory '%s'\n", argv[1]);
    return (1);
  }
  // Process each entry.
  while ((Dirent = rddir(Dir)) != NULL) {
    // Don't print inode numbers as they change
    // printf("%ld >%s<\n", Dirent->d_ino, Dirent->d_name);
    printf("%s\n", Dirent->d_name);
  }

  // Close directory and exit.
  closedir(Dir);
  return (0);
}
