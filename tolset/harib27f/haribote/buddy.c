#include "bootpack.h"

int check(int x) {
    return !(x & (x - 1));
}

unsigned int fixsize(unsigned int size) {
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    return size + 1;
}

void Buddy_new(struct Buddy *self) {
    int i, size = 0x2000;
    unsigned int node_size;

    self->size = size;
    node_size = size << 1;

    for (i = 0; i < 2 * size - 1; i++) {
        if (check(i + 1)) {
            node_size >>= 1;
        }
        self->longest[i] = node_size;
    }
}

int Buddy_alloc(struct Buddy *self, int size) {
    unsigned int index = 0;
    unsigned int node_size;
    unsigned int offset = 0;

    if (!self) {
        return -1;
    }

    if (size <= 0) {
        size = 1;
    }
    else if (!check(size)) {
        size = fixsize(size);
    }

    if (self->longest[index] < size) {
        return -1;
    }

    for (node_size = self->size; node_size != size; node_size >>= 1) {
        if (self->longest[ls(index)] >= size) {
            index = ls(index);
        }
        else if (self->longest[rs(index)] >= size){
            index = rs(index);
        }
        else {
            return -1;
        }
    }

    self->longest[index] = 0;
    offset = (index + 1) * node_size - self->size;

    while (index) {
        index = fa(index);
        self->longest[index] = maxint(self->longest[ls(index)], self->longest[rs(index)]);
    }

    return offset;
}

int Buddy_alloc_4k(struct Buddy *self, int size) {
    size = (size + 0xfff) & 0xfffff000;
    return Buddy_alloc(self, size >> 0xc) << 0xc;
}

void Buddy_free(struct Buddy *self, int offset) {
    offset >>= 0xc;
    unsigned int node_size, index = 0;
    unsigned int left_longest, right_longest;

    if (!(self && offset >= 0 && offset < self->size)) return;

    node_size = 1;
    index = offset + self->size - 1;

    for (; self->longest[index]; index = fa(index)) {
        node_size <<= 1;
        if (!index) {
            return;
        }
    }

    self->longest[index] = node_size;

    for (; index;) {
        index = fa(index);
        node_size <<= 1;

        left_longest = self->longest[ls(index)];
        right_longest = self->longest[rs(index)];

        if (left_longest + right_longest == node_size) {
            self->longest[index] = node_size;
        }
        else {
            self->longest[index] = maxint(left_longest, right_longest);
        }
    }
}

int Buddy_size(struct Buddy *self, int offset) {
    unsigned int node_size, index = 0;

    if (!(self && offset >= 0 && offset < self->size)) return -1;

    node_size = 1;
    for (index = offset + self->size - 1; self->longest[index]; index = fa(index)) {
        node_size <<= 1;
    }
    return node_size;
}

int Buddy_total(struct Buddy *self) {
    int i;
    unsigned int node_size, total = 0;

    node_size = self->size << 1;

    for (i = 0; i < 2 * self->size - 1; i++) {
        if (check(i + 1)) {
            node_size >>= 1;
        }
        if (self->longest[i] == 0) {
            if (i >= self->size - 1) {
                total++;
            }
            else if (self->longest[ls(i)] && self->longest[rs(i)]) {
                total += node_size;
            }
        }
    }
    return (0x2000 - total) << 0xc;
}

void Buddy_dump(struct Buddy *self) {
    int i, j;
    char map[MAX_SIZE + 1];
    unsigned int node_size, offset;

    if (!self) {
        return;
    }

    if (self->size > MAX_SIZE) {
        return;
    }

    for (i = 0; i <= MAX_SIZE; i++) map[i] = '_';
    node_size = self->size << 1;

    for (i = 0; i < 2 * self->size - 1; i++) {
        if (check(i + 1)) {
            node_size >>= 1;
        }
        if (self->longest[i] == 0) {
            if (i >= self->size - 1) {
                map[i - self->size + 1] = '*';
            }
            else if (self->longest[ls(i)] && self->longest[rs(i)]) {
                offset = (i + 1) * node_size - self->size;
                for (j = offset; j < offset + node_size; j++) {
                    map[j] = '*';
                }
            }
        }
    }
    map[self->size] = '\0';
    // puts(map);
}


