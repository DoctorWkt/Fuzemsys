#include <stdio.h>
#include <dirent.h>

int main(int argc, char *argv[]) {
  struct dirent *Dirent;
  DIR *Dir;

  // Ensure correct argument count.
  if (argc != 2) {
    printf("Usage: %s <dirname>\n", argv[0]);
    return(1);
  }

  // Ensure we can open directory.
  Dir = opendir(argv[1]);
  if (Dir == NULL) {
    printf("Cannot open directory '%s'\n", argv[1]);
    return(1);
  }
  // Process each entry.
  while ((Dirent = readdir(Dir)) != NULL) {
    printf("%ld %s\n", Dirent->d_ino, Dirent->d_name);
  }

  // Close directory and exit.
  closedir(Dir);
  return(0);
}
