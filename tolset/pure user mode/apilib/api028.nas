[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "api028.nas"]

		GLOBAL	_api_start_sync_test

[SECTION .text]

_api_start_sync_test:			; void api_start_sync_test(int choice, int *tcnt, int *result);
        PUSH    EDI
        PUSH    EBX
		MOV		EDX,28
		MOV		EAX,[ESP+12]			; choice
		MOV     EBX,[ESP+16]            ; *tcnt
		MOV     EDI,[ESP+20]            ; *result
		INT		0x40
		POP     EBX
		POP     EDI
		RET
