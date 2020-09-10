[FORMAT "WCOFF"]
[INSTRSET "i486p"]				; 486の命令まで使いたいという記述
[BITS 32]
[FILE "exfunc.nas"]

		GLOBAL	_getregs

[SECTION .text]

_getregs:        ; void getregs(int rtnaddr_gdtr, int rtnaddr_ldtr, int rtnaddr_ds);
        push eax
        mov eax, [esp+8]
        sgdt dword[ds:eax]
        mov eax, [esp+12]
        sldt word[ds:eax]
        mov eax, [esp+16]
        mov [ds:eax], ds
        pop eax
        ret