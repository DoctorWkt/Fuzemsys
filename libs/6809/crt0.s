		.code
start:
		.word 0x80A8
		.byte 0x04			; 6809
		.byte 0x00			; 6309 not needed
		.byte >__code			; page to load at
		.byte 0				; no hints
		.word __code_size 		; text size info
		.word __data_size		; data size info
		.word __bss_size		; bss size info
		.byte 16			; entry relative to start
		.word __end			; to help the emulator :-)
		.byte 0				; ZP not used on 6809

		jmp start2

		.code

start2:
		ldd	#0
		std	@zero
		ldd	#1
		std	@one

		; start the stack just below the emulator special area
		; lds	#$FDFF

		; we don't clear BSS since the kernel already did
		jsr ___stdio_init_vars

		; pass environ, argc and argv to main
		; pointers and data stuffed above stack by execve()
		; leax 4,s
		; stx _environ
		; leax 2,s
		; stx ___argv
		; puls x			; argc
		; ldy #_exit		; return vector
		; pshs y

		jsr	_main
		; return and exit
		stb	$FEFF

		.data

		.export _environ
_environ:	.word 0