//
// Created by egwcy on 2019/5/21.
//

#include "apilib.h"
#include <stdio.h>
#include <string.h>
void HariMain(void) {
    int tcnt = 0, z[64], i, j;
    memset(z, 0, sizeof(z));
    char str[64];
    api_cmdline(str, 64);
    for (i = 0; i < 63 && str[i] && str[i] != ' '; i++);
    for (++i; str[i] == ' '; ++i);
    switch (str[i]) {
        case '0':
            api_start_sync_test(0, &tcnt, z);
            break;
        case '1':
            // api_putstr0("ss");
            api_start_sync_test(1, &tcnt, z);
            break;
        case '2':
            z[0] = 8;
            z[1] = 1;
            z[3] = 1;
            api_start_sync_test(2, &tcnt, z);
            break;
        case '3':
            // z[0] = 8;
            // z[1] = 1;
            // z[3] = 1;
            api_start_sync_test(3, &tcnt, z);
            break;
        case '4': 
		 	api_start_sync_test(4, &tcnt, z);
		 	break;
        default:
        z[0] = -1;
        api_end();
    }
    for (;;) {
        api_start_sync_test(0xfe, 0, 0);
        j = api_getkey(0);
        if (j == 0x0a) break;
        if (str[i] < '2') {
            sprintf(str, "%d:%d\n", tcnt, z[0]);
            // api_putstr0(str);
        }
    }
    
    
    z[0] = -1;
    api_start_sync_test(0xff, 0, 0);
    api_end();
}
