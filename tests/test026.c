#include <stdio.h>
#include <string.h>

char *mesg= "Hello world\n";

int main() {
  int i= strlen(mesg);
  fwrite(mesg, strlen(mesg), 1, stdout);
  return(0);
}
