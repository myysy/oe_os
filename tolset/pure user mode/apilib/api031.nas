[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "api031.nas"]

		GLOBAL	_api_load_or_store

[SECTION .text]

; int api_load_or_store(int choice, int index, int value);
_api_load_or_store:			
        PUSH    EDI
        PUSH    EBX
		MOV		EDX,31
		MOV		EAX,[ESP+12]			; choice
		MOV     EBX,[ESP+16]            ; offset
		MOV     EDI,[ESP+20]            ; value
		INT		0x40
		POP     EBX
		POP     EDI
		RET
