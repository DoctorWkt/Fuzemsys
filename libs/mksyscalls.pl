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

__syscall:
	swi
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
