#include "apilib.h"
#include <stdio.h>
#define rw_mutex 0
#define mutex 1
#define content 0
#define read_count 1

void write(int x) {
    char s[10];
    sprintf(s, "%d\n",x);
    api_putstr0(s);
}

void HariMain(void) {
    int i = 0;
    int read_count_tmp;
    int content_tmp;
    char s[30];
    int tmp;
    do {
        api_wait_or_signal(0, mutex, 0);
        read_count_tmp = api_load_or_store(0, read_count, 0);
        read_count_tmp = api_load_or_store(0, read_count, read_count_tmp + 1);
        if (read_count_tmp == 1) {
            tmp = api_wait_or_signal(0, rw_mutex, 0);
        }
        api_wait_or_signal(1, mutex, 0);

        /* reading */
        content_tmp = api_load_or_store(0, content, 0);
        sprintf(s, "Reader read: %d\n", content_tmp);api_putstr0(s);
        for (i = 0; i < 10000; ++i);

        api_wait_or_signal(0, mutex, 0);
        read_count_tmp = api_load_or_store(0, read_count, 0);
        read_count_tmp = api_load_or_store(0, read_count, read_count_tmp - 1);
        if (read_count_tmp == 0) {
            tmp = api_wait_or_signal(1, rw_mutex, 0);
        }
        api_wait_or_signal(1, mutex, 0);

    } while (1);

}