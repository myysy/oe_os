[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "api029.nas"]

		GLOBAL	_api_xp

[SECTION .text]

_api_xp:			; void api_xp(int addr, unsigned char *rtn, int length);
        PUSH    EBX
		PUSH	EDI
		MOV		EDX,29
		MOV		EAX,[ESP+12]			; addr
		MOV     EBX,[ESP+16]            ; *rtn
		MOV		EDI,[ESP+20]
		INT		0x40
		POP		EDI
		POP     EBX
		RET
