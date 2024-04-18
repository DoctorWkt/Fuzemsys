CC          = /opt/fcc/bin/fcc -m6809
CFLAGS      = -O -c
LD          = /opt/fcc/bin/ld6809
CRT0        = /opt/fcc/lib/6809/crt0.o
NOSTDIOCRT0 = /opt/fcc/lib/6809/crt0nostdio.o
LIBS        = /opt/fcc/lib/6809/libc.a /opt/fcc/lib/6809/lib6809.a

TESTS =  test001 test002 test003 test004 test005 test006 test007 test008
TESTS += test009 test010 test011 test012 test013 test014 test015 test016
TESTS += test017 test018 test019 test020 test021 test022 test023 test024
TESTS += test025 test026 test027 test028 test029 test030

# Some tests use the crt0 which doesn't bring in the stdio library
test001: CRT= $(NOSTDIOCRT0)
test002: CRT= $(NOSTDIOCRT0) cprintf.o
test003: CRT= $(NOSTDIOCRT0) cprintf.o
test004: CRT= $(NOSTDIOCRT0) cprintf.o
test005: CRT= $(NOSTDIOCRT0) cprintf.o
test006: CRT= $(NOSTDIOCRT0) cprintf.o
test007: CRT= $(NOSTDIOCRT0) cprintf.o
test008: CRT= $(NOSTDIOCRT0) cprintf.o
test009: CRT= $(NOSTDIOCRT0) cprintf.o
test010: CRT= $(NOSTDIOCRT0) cprintf.o
test011: CRT= $(NOSTDIOCRT0) cprintf.o
test012: CRT= $(NOSTDIOCRT0) cprintf.o
test013: CRT= $(NOSTDIOCRT0) cprintf.o
test014: CRT= $(NOSTDIOCRT0) cprintf.o
test015: CRT= $(NOSTDIOCRT0) cprintf.o
test016: CRT= $(NOSTDIOCRT0) cprintf.o
test017: CRT= $(NOSTDIOCRT0) cprintf.o
test018: CRT= $(NOSTDIOCRT0) cprintf.o
test019: CRT= $(NOSTDIOCRT0) cprintf.o
test020: CRT= $(NOSTDIOCRT0) cprintf.o
test021: CRT= $(NOSTDIOCRT0) cprintf.o
test023: CRT= $(NOSTDIOCRT0) cprintf.o
test024: CRT= $(NOSTDIOCRT0) cprintf.o
test025: CRT= $(NOSTDIOCRT0) cprintf.o
test027: CRT= $(NOSTDIOCRT0) cprintf.o
test030: CRT= $(NOSTDIOCRT0) cprintf.o
#getime: CRT= $(NOSTDIOCRT0) cprintf.o

# Some tests use stdio
test022: CRT= $(CRT0) cprintf.o
test026: CRT= $(CRT0) cprintf.o
test028: CRT= $(CRT0) cprintf.o
test029: CRT= $(CRT0)

all: cprintf.o $(TESTS)

cprintf.o: cprintf.c
	$(CC) -o $@ $(CFLAGS) $<

%: %.c cprintf.o
	$(CC) -o $@.o $(CFLAGS) $<
	-$(LD) -o $@ $(CRT) $@.o $(LIBS) -m $@.map
	-rm $@.o

clean:
	rm -rf *.o *.s *.rel *.map $(TESTS)
