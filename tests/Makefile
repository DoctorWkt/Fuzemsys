CC = /opt/fcc/bin/fcc -m6809
CFLAGS = -O -c
LD = /opt/fcc/bin/ld6809
CRT0 = /opt/fcc/lib/6809/crt0.o
LIBS = /opt/fcc/lib/6809/libc.a /opt/fcc/lib/6809/lib6809.a

TESTS = test001

%: %.c
	$(CC) -o $@.o $(CFLAGS) $<
	-$(LD) -o $@ $(CRT0) $@.o $(LIBS)
	-rm $@.o

all: $(TESTS)

clean:
	rm -rf *.o *.s *.rel *.map $(TESTS)
