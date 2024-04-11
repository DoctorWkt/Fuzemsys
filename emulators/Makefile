#CFLAGS += -O2 -Wall -pedantic -g
CFLAGS +=  -Wall -pedantic -g

all: emu6809

emu6809.o: emu6809.c
	$(CC) $(CFLAGS) -c emu6809.c

e6809.o: e6809.c e6809.h
	$(CC) $(CFLAGS) -c e6809.c

d6809.o: d6809.c d6809.h e6809.h
	$(CC) $(CFLAGS) -c d6809.c

syscalls6809.o: syscalls.c
	$(CC) $(CFLAGS) -DCPU_6809 -c -o syscalls6809.o syscalls.c

mapfile.o: mapfile.c
	$(CC) $(CFLAGS) -c mapfile.c

emu6809: emu6809.o e6809.o d6809.o syscalls6809.o mapfile.o
	$(CC) $(CFLAGS) -o emu6809 emu6809.o e6809.o d6809.o \
		syscalls6809.o mapfile.o

clean:
	rm -f *.o *.map
	rm -f emu6809

install: emu6809
	cp emu6809 /opt/fcc/bin
