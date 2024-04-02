CC          = /opt/fcc/bin/fcc -m6809
CFLAGS      = -O -c
LD          = /opt/fcc/bin/ld6809
CRT0        = /opt/fcc/lib/6809/crt0.o
NOSTDIOCRT0 = /opt/fcc/lib/6809/crt0nostdio.o
LIBS        = /opt/fcc/lib/6809/libc.a /opt/fcc/lib/6809/lib6809.a

TESTS = test001 test002

# Some tests use the crt0 which doesn't bring in the stdio library
test001: CRT= $(NOSTDIOCRT0)
test002: CRT= $(NOSTDIOCRT0) cprintf.o

all: cprintf.o $(TESTS)

cprintf.o: cprintf.c
	$(CC) -o $@ $(CFLAGS) $<

%: %.c
	$(CC) -o $@.o $(CFLAGS) $<
	-$(LD) -o $@ $(CRT) $@.o $(LIBS) -m $@.map
	-rm $@.o

clean:
	rm -rf *.o *.s *.rel *.map $(TESTS)
