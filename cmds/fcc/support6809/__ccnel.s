;
;	Compare Y:D with TOS
;
	.export __ccnel

__ccnel:
	cmpy	2,s
	bne	true
	cmpd	4,s
	bne	true
	clra
	clrb
out:
	ldx	,s
	leas	6,s
	tstb
	jmp	,x
true:
	ldd	@one
	bra	out
