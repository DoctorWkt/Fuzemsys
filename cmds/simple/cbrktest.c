// cbreak test

#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>

struct termios orig_termios;	// Original blocking terminal setting

// Print out a termios struct
void print_termios(char *msg, struct termios *t) {
  int i;

  printf("%s\n", msg);
  printf("termios struct: iflag 0x%x oflag 0x%x cflag 0x%x lflag 0x%x\n",
	 t->c_iflag, t->c_oflag, t->c_cflag, t->c_lflag);

  for (i = 0; i < NCCS; i++)
    printf("%x ", t->c_cc[i]);
  printf("\n");
}

// Set the terminal back to blocking and echo
void reset_terminal(void) {
  tcsetattr(1, TCSANOW, &orig_termios);
  // printf("Just reset the terminal\n");
  // print_termios("final termios ...", &orig_termios);
}

// Put the terminal into cbreak mode with no echo
void set_cbreak() {
  struct termios t;

  // Get the original terminal settings twice,
  // one for restoration later.
  tcgetattr(1, &orig_termios);
  // print_termios("original termios ...", &orig_termios);
  if (tcgetattr(1, &t) == -1) {
    fprintf(stderr, "Cannot tcgetattr\n");
    exit(1);
  }

  t.c_lflag &= ~(ICANON | ECHO);
  t.c_lflag |= ISIG;
  t.c_iflag &= ~ICRNL;
  t.c_cc[VMIN] = 1;		// Character-at-a-time input
  t.c_cc[VTIME] = 0;		// with blocking

  // printf("Address of FUZIX termios is 0x%x\n", (void *)&t);

  if (tcsetattr(1, TCSAFLUSH, &t) == -1) {
    fprintf(stderr, "Cannot tcsetattr\n");
    exit(1);
  }
  // print_termios("cbreak termios ...", &t);
  // Ensure we reset the terminal when we exit
  atexit(reset_terminal);
}

int main(void) {
  char ch;

  printf("Type some characters, 'q' to quit\n\n");

  set_cbreak();

  while (1) {
    ch = getchar();
    if (ch == 'q')
      exit(0);
    putchar(toupper(ch));
    fflush(stdout);
  }
  exit(0);
  return (0);
}
