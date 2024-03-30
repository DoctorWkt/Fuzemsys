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

After that, no idea ... I haven't got that far as yet!
