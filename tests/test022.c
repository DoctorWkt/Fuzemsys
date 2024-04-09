// Some stdio tests

extern void printchar(int);
extern void printint(int);
void cprintf(char *fmt, ...);

#include <stdio.h>

char buf[512];

int main() {
  FILE *zin;
  size_t cnt;
  int err;

  zin= fopen("in/textfile1.txt", "r");
  if (zin == NULL) {
    cprintf("fopen() on in/textfile1.txt failed\n"); return(1);
  }

  while (1) {
    cnt= fread(buf, 1, 512, zin);
    cprintf("\ncnt is %d\n", cnt);
    if (cnt == -1 ) {
      cprintf("fread() failed\n"); return(1);
    }
    if (cnt==0) break;
    fwrite(buf, 1, cnt, stdout);
  }

  err= fclose(zin);
  if (err == -1) {
      cprintf("fclose() failed\n"); return(1);
    }
  return(0);
}
