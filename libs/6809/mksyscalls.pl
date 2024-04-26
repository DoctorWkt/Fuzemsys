#!/usr/bin/perl
use strict;
use warnings;

open(my $IN, "<", "syscall.list")
	|| die("Can't open syscall.list: $!\n");

open(my $OUT, ">", "syscalls.s")
	|| die("Can't write syscalls.s: $!\n");

my $str = <<"END";
	.export _printint
_printint:
	ldd     2,s
	std     \$FEFC
	rts

	.export _printchar
_printchar:
	ldd     2,s
	stb     \$FEFE
	rts

	.export __syscall
__syscall:
	swi
	cmpd #0		; D holds error val, X return result
	beq noerr1
	std _errno	; Save D into errno if an error
noerr1:
	tfr x,d		; Put result into D
	rts

END

print($OUT $str);
while (<$IN>) {
  chomp;
  my ($num,$name)= split(/:/);
  print($OUT "\t.export $name\n");
  print($OUT "$name:\n");
  print($OUT "\tldd #$num\n");
  print($OUT "\tjmp __syscall\n\n");
}
close($IN);
close($OUT);
