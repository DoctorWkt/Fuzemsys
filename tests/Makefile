CC = /opt/fcc/bin/fcc -m6809
CFLAGS = -O -c
LD = /opt/fcc//bin/ld6809
CRT0 = /opt/fcc//lib/6809/crt0.o
LIBS = /opt/fcc//lib/6809//libc.a /opt/fcc//lib/6809/lib6809.a

test001: test001.o
	$(LD) -o test001 $(CRT0) test001.o $(LIBS)

test001.o: test001.c
	$(CC) -o test001.o $(CFLAGS) test001.c

all: test001

clean:
	rm -rf *.o *.s *.rel *.map test001
