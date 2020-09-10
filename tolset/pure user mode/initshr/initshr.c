#include "apilib.h"
#define rw_mutex 0
#define mutex 1
#define content 0
#define read_count 1

void HariMain(void) {
    api_wait_or_signal(2, rw_mutex, 1);
    api_wait_or_signal(2, mutex, 1);
    api_load_or_store(2, read_count, 0);
    api_putstr0("finish\n");
    api_end();
}