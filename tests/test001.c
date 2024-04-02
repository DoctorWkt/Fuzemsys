#include <stdio.h>

char *str= "Hello world\n";

int main() {
  write(1, str, 12);
  return(0);
}
