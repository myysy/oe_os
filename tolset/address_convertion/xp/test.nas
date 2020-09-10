[FORMAT "WCOFF"]

[BITS 32]
[FILE "test.nas"]

		GLOBAL	_getldt

[SECTION .text]

_getldt:        ; int getldt();
        push ds
        lldt ds
        and eax, 0xffff
        mov ax, ds
        pop ds
        ret