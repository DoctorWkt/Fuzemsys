// Building up to getting printf() working.
// This checks that stdarg.h works.

#include <stdio.h>
#include <stdarg.h>

void cprintf(char *fmt, ...);

void foo(char *fmt, ...) {
  va_list ap;
  int d;
  char c;
  char *s;

  va_start(ap, fmt);
  while (*fmt) {
    if (*fmt != '%') {
      printchar(*fmt);
      fmt++;
      continue;
    }
    fmt++;

    switch (*fmt++) {
    case 's':			/* string */
      s = va_arg(ap, char *);
      cprintf("%s", s);
      break;
    case 'd':			/* int */
      d = va_arg(ap, int);
      cprintf("%d", d);
      break;
    case 'c':			/* char */
      /* need a cast here since va_arg only
         takes fully promoted types */
      c = (char) va_arg(ap, int);
      printchar(c);
      break;
    }
  }
  va_end(ap);
}

int main() {
  foo("Hello world\n");
  foo("Integer %d\n", 23);
  foo("Character %c\n", 'w');
  foo("String %s\n", "foo bar");
  return(0);
}
