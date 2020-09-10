#include "bootpack.h"
void avoid_sleep() {
	struct TASK *now_task;
	now_task = task_now();
	now_task->flags = 2;
}


int test_and_set(int *target) {
    int eflags;
    eflags = io_load_eflags();
    io_cli();
    int tmp = *target;
    *target = 0xff;
    io_store_eflags(eflags);
    return tmp;
}
void Swap(int *a, int *b) {
    int eflags;
    eflags = io_load_eflags();
    io_cli();
    int tmp = *a;
    *a = *b;
    *b = tmp;
    io_store_eflags(eflags);
}
void sem_signal(int *x) {
    int eflags;
    eflags = io_load_eflags();
    io_cli();
    (*x)++;
    io_store_eflags(eflags);
}
void sem_wait(int *x) {
    
    int eflags;
    while ((*x) <= 0)
        avoid_sleep();
    eflags = io_load_eflags();
    io_cli();
    (*x)--;
    io_store_eflags(eflags);
}

struct TASK *create_task(struct MEMMAN *man, void (*f)(void *), void *args) {
    struct TASK *tst = task_alloc();
    tst->cons_stack = memman_alloc_4k(man, 64 * 1024);
    tst->tss.esp = tst->cons_stack + 64 * 1024 - 8;
    tst->tss.eip = (int)f;
    tst->tss.es = 1 * 8;
    tst->tss.cs = 2 * 8;
    tst->tss.ss = 1 * 8;
    tst->tss.ds = 1 * 8;
    tst->tss.fs = 1 * 8;
    tst->tss.gs = 1 * 8;
    *((int *)(tst->tss.esp + 4)) = (int)args;
    return tst;
}

void resultoutput(struct Rarg *args) {
    char buf[64];
    char tag = 0;
    for (;;) {
        avoid_sleep();
        sprintf(buf, "%d:%d\n", *(args->x), *(args->z));
        if (args->tsk->cons) tag = 1;
        cons_putstr0(args->tsk->cons, buf);
        if (args->tsk->cons == 0 && tag) break;
    }
}


void test_race_condition0(struct Rarg *args) {
    int *x = args->x,
        *y = args->y,
        *z = args->z,
        *lock = args->lock;
    int i, tmp;
    cons_putstr0(args->tsk->cons, "RC\n");
    for (;;) {
        tmp = *x;
        (*x)++;
        i = 5;
        while (i--)
            avoid_sleep();
        if ((*x) - tmp > 1) {
            *z = 1;
        }
    }
}
void test_race_condition1(struct Rarg *args) {
    int *x = args->x,
            *y = args->y,
            *z = args->z,
            *lock = args->lock;
    int i, tmp;
    for (;;) {
        while (test_and_set(lock))
            avoid_sleep();
        tmp = *x;
        (*x)++;
        i = 5;
        while (i--);
        if ((*x) - tmp > 1) {
            *z = 1;
        }
        *lock = 0;
    }
}
void test_race_condition3(struct Rarg *args) {
    int *x = args->x,
            *y = args->y,
            *z = args->z;
    struct TASK *tsk = args->tsk;
    char buffer[128];
    struct semaphore *s = (struct semaphore *)args->lock;
    sprintf(buffer, "%d entering\n", args->y);
    cons_putstr0(tsk->cons, buffer);
    int i, tmp;
    for (;;) {
        if (z == -1) break;
        // avoid_sleep();
        // sem_wait(s, tsk->cons);
        sprintf(buffer, "%d working\n", args->y);
        cons_putstr0(tsk->cons, buffer);
        tmp = *x;
        (*x)++;
        i = 5;
        while (i--)
            avoid_sleep();
        if ((*x) - tmp > 1) {
            if (z != -1) *z = 1;
        }
        // semaphore_signal(s);
    }
}


void producer(struct BBParg *args) {
    // avoid_sleep();
    int *empty = args->empty,
            *mutex = args->mutex,
            *full = args->full,
            id = args->id;
    struct FIFO32 *fifo = args->fifo;
    struct TASK *task = args->task;
    int cnt = 2, i;
    char tag;
    char str[128];
    while (1) {
        tag = 0;
        while (!tag) {
            tag = 1;
            for (i = 2; i * i <= cnt; i++) {
                if (cnt % i == 0) {
                    tag = 0;
                    break;
                }
            }
            if (!tag) cnt++;
            if (cnt > 0x3f3f3f) cnt = 2;
        }
        sem_wait(empty);
        sem_wait(mutex);
        fifo32_put(fifo, cnt++);
        sprintf(str, "pproc gens %d\n", cnt - 1);
		cons_putstr0(task->cons, str);
        sem_signal(mutex);
        sem_signal(full);
    }
}
void consumer(struct BBParg *args) {
//    avoid_sleep();
    int *empty = args->empty,
            *mutex = args->mutex,
            *full = args->full,
            id = args->id,
            *con_mutex = args->con_mutex;
    struct FIFO32 *fifo = args->fifo;
    struct TASK *task = args->task;
    int tmp, i;
    char str[128];
    while (1) {
        while (task->cons == 0);
            avoid_sleep();
        sem_wait(full);
        sem_wait(mutex);
        tmp = fifo32_get(fifo);
        sem_signal(mutex);
        sem_signal(empty);
        sprintf(str, "cproc %d fetched %d\n", id, tmp);
        sem_wait(con_mutex);
        cons_putstr0(task->cons, str);
        sem_signal(con_mutex);
    }
}