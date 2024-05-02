// Trying to get readdir to work.
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <alloc.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

void cprintf(char *fmt, ...);

// #define WKT_VERSION 1

#if 0
static struct __dirent *ddnext(DIR * dir) {
#ifdef WKT_VERSION
  int cnt;

  if (dir == NULL)
    return (NULL);
  cnt = read(dir->dd_fd, dir->_priv.buf, sizeof(struct __dirent));
  if (cnt <= 0)
    return (NULL);
  return ((struct __dirent *) dir->_priv.buf);
#else
  uint8_t oldnext;

  if (dir->_priv.next == dir->_priv.last) {
    int l = read(dir->dd_fd, dir->_priv.buf, sizeof(dir->_priv.buf));
    if (l <= 0)
      return NULL;
    l /= 32;
    dir->_priv.last = l;
    dir->_priv.next = 0;
  }
  oldnext = dir->_priv.next;
  // ++ on dir->_priv.next doesn't yet work
  dir->_priv.next = dir->_priv.next + 1;
  return (struct __dirent *) (dir->_priv.buf + 32 * oldnext);
#endif
}

struct dirent *rddir(DIR * dir) {
  struct __dirent *direntry;
  register struct dirent *buf;

  if (dir == NULL) {
    errno = EFAULT;
    return NULL;
  }

  do {
    direntry = ddnext(dir);
    if (direntry == NULL)
      return NULL;
  } while (direntry->d_name[0] == 0);

  buf = &dir->_priv.de;
  buf->d_ino = direntry->d_ino;
  buf->d_off = -1;		/* FIXME */
  buf->d_reclen = 31;
  dir->dd_loc += (buf->d_reclen + 1);
  strncpy(buf->d_name, (char *) direntry->d_name, 31);
  buf->d_name[30] = 0;
  return buf;
}
#endif

int main(int argc, char *argv[]) {
  struct dirent *Dirent;
  DIR *Dir;

  // Ensure we can open directory.
  Dir = opendir("in");
  if (Dir == NULL) {
    printf("Cannot open directory '%s'\n", argv[1]);
    return (1);
  }
#if 0

  // Process each entry.
  while ((Dirent = rddir(Dir)) != NULL) {
    // Don't print inode numbers as they change
    // printf("%ld >%s<\n", Dirent->d_ino, Dirent->d_name);
    printf("%s\n", Dirent->d_name);
  }

  // Close directory and exit.
  closedir(Dir);
#endif
  return (0);
}
