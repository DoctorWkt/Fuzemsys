# Fuzemsys

This is a spin off of the emulators in the
[Fuzix Compiler Kit](https://github.com/EtchedPixels/Fuzix-Compiler-Kit);
I am adding
FUZIX system call support to them. Eventually (hopefully) the changes will
get merged back into the Fuzix Compiler Kit.

## Usage

Firstly get:

+ https://github.com/EtchedPixels/Fuzix-Bintools
+ https://github.com/EtchedPixels/Fuzix-Compiler-Kit

Compile Bintools and install into `/opt/fcc`, then compile the compiler kit
and install there as well.

Do a `make; make install` here to build the 6809 and Z80 emulators and install
them into `/opt/fcc/bin/`. This will also install the 6809/Z80 include files
and the 6809/Z80 libraries.

## Tests

With the emulators installed, change into the `tests` directory and do a
`make -f Makefile.6809` to build the 6809 test executables. Now, for example, you can run:

```
$ emu6809 test001
```

and you should see "Hello world" written to standard output. There is a `Makefile.z80`
as well for the Z80 test executables.

There is a `runtests` script in `tests/` that runs the executables using
a specific emulator and checks that they behave as expected. Example:

```
$ ./runtests 6809    (or ./runtests z80)
test001.c: OK
test002.c: OK
test003.c: OK
...
test032.c: OK
```

## Commands

There are some commands in the `cmds/` directory which will run under the
emulator. Use the appropriate `Makefile` for the CPU that you wish. See below about emulated filesystems.

## Emulator Usage

The `emu6809` and `emuz80` emulators have support for map files and include a built-in monitor.
Here are the usage details for `emu6809` (the same as `emuz80`):

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
Symbols + offset, e.g. _printf+23, _printf+$100
```

## Emulated Filesystems

The emulators have the concept of an emulated filesystem. Go into
the `cmds` directory and do a `make install`. This will create two
directories, `build/6809/` and `build/z80/` at the top level of the
repository. Each one contains a `bin/` directory with the available
application binaries.

Now set the `FUZIXROOT` environment to one of the "build" directories, e.g.

```
export FUZIXROOT=<mumble>/build/z80
```

You can now `cd $FUZIXROOT` and start an emulated shell up:

```
cd $FUZIXROOT
emuz80 bin/sh
$
```

The `$` sign is the emulated shell prompt. From here you can explore
the emulated filesystem, e.g.

```
$ ls -l
drwxr-xr-x   2 1000     1000         4096 May 08 03:56 bin
$ cd bin
$ ls -l
-rwxr-xr-x   1 1000     1000        23970 May 08 03:56 ar
-rwxr-xr-x   1 1000     1000        10551 May 08 03:56 banner
-rwxr-xr-x   1 1000     1000         5669 May 08 03:56 basename
-rwxr-xr-x   1 1000     1000         7781 May 08 03:56 cal
-rwxr-xr-x   1 1000     1000         1931 May 08 03:56 cat
-rwxr-xr-x   1 1000     1000         5453 May 08 03:56 cmp
-rwxr-xr-x   1 1000     1000        29256 May 08 03:56 cpp
-rwxr-xr-x   1 1000     1000        10849 May 08 03:56 cut
-rwxr-xr-x   1 1000     1000         8933 May 08 03:56 date
...
-rwxr-xr-x   1 1000     1000        10203 May 08 03:56 sh
-rwxr-xr-x   1 1000     1000        18139 May 08 03:56 sort
-rwxr-xr-x   1 1000     1000        13611 May 08 03:56 tail
-rwxr-xr-x   1 1000     1000        11693 May 08 03:56 tsort
$
```

Filenames starting with `/` are translated to start with `$FUZIXROOT`.
The exception are emulated binaries that don't exist. For example,
there is no `vi`. From within the emulator, if you try to run `/usr/bin/vi`
from the shell then:

 - the exec of `$FUZIXROOT/usr/bin/vi` fails
 - instead, we try to exec `/usr/bin/vi` which succeeds

This allows you to run native binary executables from within the emulators
as well as the emulated binary executables.

The emulated shell has an implicit `PATH` which is `/bin:/usr/bin`.
So you can simply type in `vi` and the shell will run the native `vi`
which lives in `/usr/bin`.
