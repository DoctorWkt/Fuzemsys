;
;	Compare Y:D with TOS
;
	.export __cceql

__cceql:
	cmpy	2,s
	bne	false
	cmpd	4,s
	bne	false
	ldd	@one
out:
	ldx	,s
	leas	6,s
	tstb
	jmp	,x
false:
	clra
	clrb
	bra	out
