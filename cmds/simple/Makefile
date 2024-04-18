CC          = /opt/fcc/bin/fcc -m6809
CFLAGS      = -O -c
LD          = /opt/fcc/bin/ld6809
CRT0        = /opt/fcc/lib/6809/crt0.o
NOSTDIOCRT0 = /opt/fcc/lib/6809/crt0nostdio.o
LIBS        = /opt/fcc/lib/6809/libc.a /opt/fcc/lib/6809/lib6809.a

BINS=cat echo ln mkdir oldgrep rm prdir

# Programs that don't use stdio
cat: CRT= $(NOSTDIOCRT0) cprintf.o
echo: CRT= $(NOSTDIOCRT0) cprintf.o
ln: CRT= $(NOSTDIOCRT0) cprintf.o
mkdir: CRT= $(NOSTDIOCRT0) cprintf.o
oldgrep: CRT= $(NOSTDIOCRT0) cprintf.o
rm: CRT= $(NOSTDIOCRT0) cprintf.o

# Programs that use stdio
basename: CRT= $(CRT0)
ls: CRT= $(CRT0)
prdir: CRT= $(CRT0)

all: cprintf.o $(BINS)

cprintf.o: cprintf.c
	$(CC) -o $@ $(CFLAGS) $<

%: %.c cprintf.o
	$(CC) -o $@.o $(CFLAGS) $<
	-$(LD) -o $@ $(CRT) $@.o $(LIBS) -m $@.map
	-rm $@.o

clean:
	rm -rf *.o *.s *.rel *.map $(BINS)
