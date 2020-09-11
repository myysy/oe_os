#include "bootpack.h"

#define EFLAGS_AC_BIT 0x00040000
#define CR0_CACHE_DISABLE 0x60000000
#define TYPE 2
unsigned int memtest(unsigned int start, unsigned int end) {
    char flg486 = 0;
    unsigned int eflg, cr0, i;

    eflg = io_load_eflags();
    eflg |= EFLAGS_AC_BIT; /* AC-bit = 1 */
    io_store_eflags(eflg);
    eflg = io_load_eflags();
    if ((eflg & EFLAGS_AC_BIT) != 0) {
        flg486 = 1;
    }
    eflg &= ~EFLAGS_AC_BIT; /* AC-bit = 1 */
    io_store_eflags(eflg);

    if (flg486 != 0) {
        cr0 = load_cr0();
        cr0 |= CR0_CACHE_DISABLE;
        store_cr0(cr0);
    }

    i = memtest_sub(start, end);

    if (flg486 != 0) {
        cr0 = load_cr0();
        cr0 &= ~CR0_CACHE_DISABLE;
        store_cr0(cr0);
    }

    return i;
}

void memman_init(struct MEMMAN *man) {
    man->frees = 0;
    man->maxfrees = 0;
    man->lostsize = 0;
    man->losts = 0;
    return;
}

unsigned int memman_total(struct MEMMAN *man) {
    unsigned int i, t = 0;
    for (i = 0; i < man->frees; i++) {
        t += man->free[i].size;
    }
    return t;
}

unsigned int memman_alloc(struct MEMMAN *man, unsigned int size) {
    unsigned int i, a;
    struct FREEINFO tmp;
#if TYPE == 1
    for (i = 0; i < man->frees; i++) {
        if (man->free[i].size >= size) {
            a = man->free[i].addr;
            man->free[i].addr += size;
            man->free[i].size -= size;
            if (man->free[i].size == 0) {
                man->frees--;
                for (; i < man->frees; i++) {
                    man->free[i] = man->free[i + 1];
                }
            }
            return a;
        }
    }
#elif TYPE == 2
    for (i = 0; i < man->frees; i++) {
        if (man->free[i].size >= size) {
            a = man->free[i].addr;
            tmp.addr = man->free[i].addr + size;
            tmp.size = man->free[i].size - size;
            man->frees--;
            for (; i < man->frees; i++) {
                man->free[i] = man->free[i + 1];
            }
            if (tmp.size) {
                memman_free(man, tmp.addr, tmp.size);
            }
            return a;
        }
    }
#elif TYPE == 3
    if (man->frees) {
        if (man->free[0].size >= size) {
            a = man->free[0].addr;
            tmp.addr = man->free[0].addr + size;
            tmp.size = man->free[0].size - size;
            man->frees--;
            for (i = 0; i < man->frees; i++) {
                man->free[i] = man->free[i + 1];
            }
            if (tmp.size) {
                memman->free(man, tmp.addr, tmp.size);
            }
            return a;
        }
    }
#endif
    return 0;
}

int memman_free(struct MEMMAN *man, unsigned int addr, unsigned int size) {
    int i, j;
    for (i = 0; i < man->frees; i++) {
#if TYPE == 1
        if (man->free[i].addr > addr) break;
#elif TYPE == 2
        if (man->free[i].size > size) break;
#elif TYPE == 3
        if (man->free[i].size < size) break;
#endif
    }

	

    if (i > 0) {
        if (man->free[i - 1].addr + man->free[i - 1].size == addr) {
            man->free[i - 1].size += size;
            if (i < man->frees) {
                if (addr + size == man->free[i].addr) {
                    man->free[i - 1].size += man->free[i].size;
                    man->frees--;
                    for (; i < man->frees; i++) {
                        man->free[i] = man->free[i + 1];
                    }
                }
            }
            return 0;
        }
    }
    if (i < man->frees) {
        if (addr + size == man->free[i].addr) {
            man->free[i].addr = addr;
            man->free[i].size += size;
            return 0;
        }
    }
    if (man->frees < MEMMAN_FREES) {
        for (j = man->frees; j > i; j--) {
            man->free[j] = man->free[j - 1];
        }
        man->frees++;
        if (man->maxfrees < man->frees) {
            man->maxfrees = man->frees;
        }
        man->free[i].addr = addr;
        man->free[i].size = size;
        return 0;
    }
    man->losts++;
    man->lostsize += size;
    return -1;
}

void QuickSort_addr(struct MEMMAN *man, int low, int high) {
    if (low < high) {
        int i = low;
        int j = high;
        int k = man->free[low].addr;
        int kk = man->free[low].size;
        while (i < j) {
            // 从右向左找第一个小于k的数
            while (i < j && man->free[j].addr >= k) {
                j--;
            }

            if (i < j) {
                man->free[i++] = man->free[j];
            }
            // 从左向右找第一个大于等于k的数
            while (i < j && man->free[i].addr < k) {
                i++;
            }

            if (i < j) {
                man->free[j--] = man->free[i];
            }
        }

        man->free[i].addr = k;
        man->free[i].size = kk;

        // 递归调用
        QuickSort_addr(man, low, i - 1);   // 排序k左边
        QuickSort_addr(man, i + 1, high);  // 排序k右边
    }
}

void QuickSort_size(struct MEMMAN *man, int low, int high) {
    if (low < high) {
        int i = low;
        int j = high;
        int k = man->free[low].addr;
        int kk = man->free[low].size;
        while (i < j) {
            // 从右向左找第一个小于k的数
            while (i < j && man->free[j].size >= kk) {
                j--;
            }

            if (i < j) {
                man->free[i++] = man->free[j];
            }
            // 从左向右找第一个大于等于k的数
            while (i < j && man->free[i].size < kk) {
                i++;
            }

            if (i < j) {
                man->free[j--] = man->free[i];
            }
        }

        man->free[i].addr = k;
        man->free[i].size = kk;

        // 递归调用
        QuickSort_size(man, low, i - 1);   // 排序k左边
        QuickSort_size(man, i + 1, high);  // 排序k右边
    }
}
