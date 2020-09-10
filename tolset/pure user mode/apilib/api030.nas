[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "api030.nas"]

		GLOBAL	_api_getTSS32

[SECTION .text]

_api_getTSS32:			; void api_getTSS32(int rtnaddr);
		MOV		EDX,30
		MOV		EAX,[ESP+4]			; rtnaddr
		INT		0x40
		RET
