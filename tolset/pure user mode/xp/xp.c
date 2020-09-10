#include "apilib.h"
#include <stdio.h>
#define ADR_GDT			0x00270000
#define LIMIT_GDT		0x0000ffff
struct SEGMENT_DESCRIPTOR {
	short limit_low, base_low;
	char base_mid, access_right;
	char limit_high, base_high;
};
struct TSS32 {
	int backlink, esp0, ss0, esp1, ss1, esp2, ss2, cr3;
	int eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
	int es, cs, ss, ds, fs, gs;
	int ldtr, iomap;
};
/* avoid alignment causing uncontinuous storage */
#pragma pack(2)
struct GDTR {
    unsigned short limit;
    unsigned int base;
};
#pragma pack()
typedef unsigned short TABLE_SELECTOR;

unsigned int get_base(struct SEGMENT_DESCRIPTOR desc) {
    return (desc.base_high << 24) | (desc.base_mid << 16) | desc.base_low;
}

void HariMain(void) {
    static int x = 233, y, dumb = 0;
    static char s[128];
    static struct SEGMENT_DESCRIPTOR ldtdesc, lnedesc;
    static struct TSS32 tss;
    static struct GDTR gdtr;
    static unsigned int ldt_limit, ldt_base, ds = 0;
    static unsigned short gdt_limit;
    static unsigned int gdt_base, lne_base, realaddr;
    static unsigned int ldtr;
    
    getregs((int)&gdtr, (int)&ldtr, (int)&ds);
    gdt_limit = gdtr.limit;
    gdt_base = gdtr.base;
    char *xx = &gdtr;

    /* another way of getting necessary register value */
    // gdt_base = ADR_GDT;
    // gdt_limit = LIMIT_GDT
    // api_getTSS32(&tss);
    // ldtr = tss.ldtr;
    // ds = tss.ds;


    sprintf(s, "&x = 0x%08x\n&gdt_limit = 0x%08x\n&gdt_base = 0x%08x\n", &x, &gdt_limit, &gdt_base);
    api_putstr0(s);

    sprintf(s, "GDTBASE 0x%08x\nGDTLIMIT 0x%08x\n", gdt_base, gdt_limit);
    api_putstr0(s);

    sprintf(s, "LDTR 0x%08x\nTI=%d\n", ldtr, (ldtr & 4) ? 1 : 0);
    api_putstr0(s);

    api_xp(gdt_base + (ldtr & 0xfffffff8), &ldtdesc, sizeof(ldtdesc));
    ldt_base = (ldtdesc.base_high << 24) | (ldtdesc.base_mid << 16) | ldtdesc.base_low;
    
    sprintf(s, "LDTDESC 0x%08x 0x%08x\n", *((int*)&ldtdesc), *(((int*)&ldtdesc) + 1));
    api_putstr0(s);

    sprintf(s, "LDTBASE 0x%08x\n", ldt_base);
    api_putstr0(s);

    sprintf(s, "ds 0x%08x\nTI=%d\n", ds, (ds & 4) ? 1 : 0);
    api_putstr0(s);

    api_xp(ldt_base + (ds & 0xfffffff8), &lnedesc, sizeof(lnedesc));
    lne_base = (lnedesc.base_high << 24) | (lnedesc.base_mid << 16) | lnedesc.base_low;
    
    sprintf(s, "LNEDESC 0x%08x 0x%08x\n", *((int*)&lnedesc), *(((int*)&lnedesc) + 1));
    api_putstr0(s);

    sprintf(s, "LNEBASE 0x%08x\n", lne_base);
    api_putstr0(s);

    realaddr = lne_base + (int)&x;
    sprintf(s, "REALADDR 0x%08x\n", realaddr);
    api_putstr0(s);

    api_xp(realaddr, &y, sizeof(y));
    sprintf(s, "CHECK %d %d\n", x, y);
    api_putstr0(s);

    while (1) {
        dumb++;
    }
    api_end();
}