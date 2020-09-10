; func
; TAB=4
[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "func.nas"]

        GLOBAL _get_ds

[SECTION .text]

_get_ds:    ; int get_ds()
        MOV     EAX, DS
        RET