		.export ___stdio_init_vars
		.export _main
		.export _exit
		.export _environ
		.export ___argv

		.code

start:
		.word 0x80A8
		.byte 0x04			; 6809
		.byte 0x00			; 6309 not needed
		.byte 0x100			; page to load at XXX FIX
		.byte 0				; no hints
		.word 0 			;gives us header + all text segments XXX FIX
		.word 0				; gives us data size info XXX FIX
		.word 0				; bss size info XXX FIX
		.byte 16			; entry relative to start
		.byte 0				; no chmem hint
		.byte 0				; no stack hint
		.byte 0				; ZP not used on 6809

		jmp start2

		.code

start2:
		; we don't clear BSS since the kernel already did
		jsr ___stdio_init_vars

		; pass environ, argc and argv to main
		; pointers and data stuffed above stack by execve()
		leax 4,s
		stx _environ
		leax 2,s
		stx ___argv
		puls x			; argc
		ldy #_exit		; return vector
		pshs y
		jmp _main		; go

		.data

_environ:	.word 0
