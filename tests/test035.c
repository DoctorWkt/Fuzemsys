// getenv() test
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
  printf("%s %s\n", "TERM", getenv("TERM"));
  return (0);
}
