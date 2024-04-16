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
