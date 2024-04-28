#!/usr/bin/perl
use strict;
use warnings;

open(my $IN, "<", "../syscall.list")
	|| die("Can't open ../syscall.list: $!\n");

open(my $OUT, ">", "syscalls.s")
	|| die("Can't write syscalls.s: $!\n");

my $str = <<"END";
	.export _printint
_printint:
	ld	a,h
	out	(0xFB),a
	ld	a,l
	out	(0xFC),a
	ret

	.export _printchar
_printchar:
	out	(0xFE),a
	ret

	.export __syscall
__syscall:
	ex      (sp), hl                ; hl is now return addr
                                        ; stack is syscall
        ex      de, hl                  ; save return addr in de
        rst     #0x30
        ex      de, hl                  ; undo the magic
        ex      (sp), hl
        ex      de, hl                  ; return with HL
        ret     nc                      ; ok
        ld      (_errno), hl            ; error path
        ld      hl, #0xffff
        ret

END

print($OUT $str);
while (<$IN>) {
  chomp;
  my ($num,$name)= split(/:/);
  print($OUT "\t.export $name\n");
  print($OUT "$name:\n");
  print($OUT "\tld a,$num\n");
  print($OUT "\tjp __syscall\n\n");
}
close($IN);
close($OUT);
