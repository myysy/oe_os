[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "api032.nas"]

		GLOBAL	_api_wait_or_signal

[SECTION .text]

_api_wait_or_signal:			; int _api_wait_or_signal(int choice, int indexs, int value);
		PUSH    EDI
        PUSH    EBX
		MOV		EDX,32
		MOV		EAX,[ESP+12]			; choice
		MOV     EBX,[ESP+16]            ; index
		MOV     EDI,[ESP+20]            ; value
		INT		0x40
		POP     EBX
		POP     EDI
		RET
