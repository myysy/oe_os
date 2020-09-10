#include "apilib.h"
#include <stdio.h>
#define rw_mutex 0
#define content 0

void HariMain(void) {
    int x = 1;
    char s[30];
    do {
        api_wait_or_signal(0, rw_mutex, 0);

        api_load_or_store(1, content, x);
        sprintf(s, "Writer write %d.\n", x);
        api_putstr0(s);

        int i = 0;
        for(; i < 100000000; ++i);

        api_wait_or_signal(1, rw_mutex, 0);
    } while(++x);
    api_end();
}