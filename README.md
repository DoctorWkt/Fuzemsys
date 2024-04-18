# Fuzemsys

This is a spin off of the emulators in the FUZIX Compiler Kit; I am adding
FUZIX system call support to them. Eventually (hopefully) the changes will
get merged back into the FUZIX Compiler Kit.

## Usage

Firstly get:

+ https://github.com/EtchedPixels/Fuzix-Bintools
+ https://github.com/EtchedPixels/Fuzix-Compiler-Kit

Compile Bintools and install into `/opt/fcc`, then compile the compiler kit
and install there as well.

Do a `make; make install` here to build at least the 6809 emulator and install
this into `/opt/fcc/bin/emu6809`. This will also install the 6809 include files
and the 6809 library.

## Tests

With the emulator installed, change into the `tests` directory and do a `make` to build the
test executables. Now, for example, you can run:

```
$ emu6809 test001
```

and you should see "Hello world" written to standard output.

There is a `runtests` script in `tests/` that runs the executables using
the 6809 emulator and checks that they behave as expected. Example:

```
$ ./runtests
test001.c: OK
test002.c: OK
test003.c: OK
```

## Commands

There are some commands in the `cmds/` directory which will run under the
emulator. At present (April 16 2024) the stdio library mostly works but
`printf()` isn't working yet (although `stdarg.h` works).

## Emulator Usage

The `emu6809` emulator now has support for map files and a built-in monitor.
Here is the emulator's usage details:

```
Usage: emu6809 [-M] [-d logfile] [-m mapfile] [-b addr] executable <arguments>

	-d: write debugging information to logfile
	-m: load a mapfile with symbol information
	-M: start in the monitor
	-b: set breakpoint at address (decimal or $hex)

	If the FUZIXROOT environment variable is set,
	use that as the executable's root directory.
```

and here are the monitor instructions:

```
Monitor usage:

s, step <num>             - execute 1 or <num> instructions
x, exit                   - exit the monitor, back to running
q, quit                   - quit the emulation
g, go <addr>              - start execution at address
p, print <addr> [<addr2>] - dump memory in the address range
d, dis <addr> [<addr2>]   - disassemble memory in the address range
w, write <addr> <value>   - overwrite memory with value
b, brk [<addr>]           - set instruction breakpoint at <addr> or
                            show list of breakpoints
wb, wbrk <addr>           - set a write breakpoint at <addr>
nb, nbrk [<addr>]         - remove breakpoint at <addr>, or all

Addresses and Values

Decimal literals start with [0-9], e.g. 23
Hexadecimal literals start with $, e.g. $1234
Symbols start with _ or [A-Za-z], e.g. _printf
Synbols + offset, e.g. _printf+23, _printf+$100
```
