CC          = /opt/fcc/bin/fcc -m6809
CFLAGS      = -O -c
LD          = /opt/fcc/bin/ld6809
CRT0        = /opt/fcc/lib/6809/crt0.o
NOSTDIOCRT0 = /opt/fcc/lib/6809/crt0nostdio.o
LIBS        = /opt/fcc/lib/6809/libc.a /opt/fcc/lib/6809/lib6809.a

TESTS = test001

# Some tests use the crt0 which doesn't bring in the stdio library
test001: CRT= $(NOSTDIOCRT0)

%: %.c
	$(CC) -o $@.o $(CFLAGS) $<
	-$(LD) -o $@ $(CRT) $@.o $(LIBS)
	-rm $@.o

all: $(TESTS)

clean:
	rm -rf *.o *.s *.rel *.map $(TESTS)
