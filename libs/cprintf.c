// cprintf.c comes from xv6 and is covered by this license.
//
// Copyright (c) 2006-2018 Frans Kaashoek, Robert Morris, Russ Cox,
//                         Massachusetts Institute of Technology
// 
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

extern void printchar(int);
extern void printint(int);

static void print_int(int xx, int base, int sign) {
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  unsigned int x;

  if (sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[(unsigned int) (x % base)];
  } while ((x /= base) != 0);

  if (sign)
    buf[i++] = '-';

  while (--i >= 0)
    printchar(buf[i]);
}

// Print to the console. only understands %d, %x, %p, %s.
void cprintf(char *fmt, ...) {
  int i, c, locking;
  unsigned int *argp;
  char *s;

  if (fmt == 0)
    // panic("null fmt");
    return;

  argp = (unsigned int *) (void *) (&fmt + 1);

  for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {
    if (c != '%') {
      printchar(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if (c == 0)
      break;
    switch (c) {
    case 'c':
      printchar((char) (*argp & 0xff)); argp++;
      break;
    case 'd':
      print_int(*argp++, 10, 1);
      break;
    case 'o':
      print_int(*argp++, 8, 1);
      break;
    case 'x':
    case 'p':
      print_int(*argp++, 16, 0);
      break;
    case 's':
      if ((s = (char *) *argp++) == 0)
	s = "(null)";
      for(; *s; s++)
        printchar(*s);
      break;
    case '%':
      printchar('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      printchar('%');
      printchar(c);
      break;
    }
  }
}
