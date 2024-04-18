// printf() now works!

#include <stdio.h>

long fred= 12034;

int main() {
  printf("Hello world\n");
  printf("Integer %d\n", 23);
  printf("Long %ld\n", fred);
  printf("Character %c\n", 'w');
  printf("String %s\n", "foo bar");
  return(0);
}
