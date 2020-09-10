#include "apilib.h"
#include <stdio.h>

void HariMain(void) {
    char s[10];
    int tmp, cha;
    tmp = api_load_or_store(1, 0, 0);
    int i;
    for (i = 0; i < 10000; ++i) {
        // api_wait_or_signal(0, 0, 0);

        tmp = api_load_or_store(0, 0, 0);
        cha = api_load_or_store(2, 0, 0);
        
        // api_wait_or_signal(1, 0, 0);
        
        sprintf(s, "%d %d\n", cha - tmp, cha);
        api_putstr0(s);
    }
    api_end();
}