		.code

.export __text		; We will need this for the syscall changes
			; to get at the stubs
__text:
		.word 0x80A8		; Fuzix executable
		.byte 1			; 8080 series
		.byte 0x02		; Z80 feature set
		.byte >__code		; page to load at
		.byte 0			; No hints

		.word __data		; code + literal (0 based)
		.word __data_size	; data
		.word __bss_size	; bss size

		.byte 16		; Run 16 bytes in
		.word __end		; To help the emulator
		.byte 0			; No ZP on 8080

start2:
		call __ldword6		; Copy the envp pointer
        	ld (_environ+0), hl	; to _environ
		call ___stdio_init_vars
		call _main		; go
		push hl
		call _exit
					
.export _environ
.export _errno
_environ:	.word 0
_errno:		.word 0

