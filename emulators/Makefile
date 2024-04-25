#CFLAGS += -O2 -Wall -pedantic -g
CFLAGS +=  -Wall -pedantic -g
LIBS= -lreadline

all: emu6809 emuz80

emu6809.o: emu6809.c
	$(CC) $(CFLAGS) -c emu6809.c

e6809.o: e6809.c e6809.h
	$(CC) $(CFLAGS) -c e6809.c

d6809.o: d6809.c d6809.h e6809.h
	$(CC) $(CFLAGS) -c d6809.c

emuz80.o: emuz80.c
	$(CC) $(CFLAGS) -c emuz80.c

z80dis.o: z80dis.c z80dis.h
	$(CC) $(CFLAGS) -c z80dis.c

libz80/libz80.o:
	(cd libz80; make)

syscalls6809.o: syscalls.c
	$(CC) $(CFLAGS) -DCPU_6809 -c -o syscalls6809.o syscalls.c

mapfile.o: mapfile.c
	$(CC) $(CFLAGS) -c mapfile.c

emumon.o: emumon.c
	$(CC) $(CFLAGS) -DCPU_6809 -c emumon.c

emu6809: emu6809.o e6809.o d6809.o syscalls6809.o mapfile.o emumon.o
	$(CC) $(CFLAGS) -o emu6809 emu6809.o e6809.o d6809.o \
		syscalls6809.o mapfile.o emumon.o \
		$(LIBS)

emuz80: emuz80.o z80dis.o libz80/libz80.o
	$(CC) $(CFLAGS) -o emuz80 emuz80.o z80dis.o libz80/libz80.o

clean:
	rm -f *.o *.map
	rm -f emu6809 emuz80
	(cd libz80; make clean)

install: emu6809
	cp emu6809 /opt/fcc/bin
