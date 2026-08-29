/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: stdlib.c
 *
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#define ARENA_SIZE (32UL * 1024 * 1024)
#define MALLOC_ALIGNMENT 16

typedef struct block {
    size_t size;
    struct block *next;
    int free;
} block_t;

static block_t *blocks;
static int libc_errno;

int *__errno_location(void) {
    return &libc_errno;
}

static size_t align_size(size_t size) {
    return (size + MALLOC_ALIGNMENT - 1) & ~(size_t)(MALLOC_ALIGNMENT - 1);
}

static int heap_init(void) {
    if (blocks) {
        return 1;
    }

    blocks = mmap(NULL, ARENA_SIZE, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANON, -1, 0);
    if (blocks == MAP_FAILED) {
        blocks = NULL;
        return 0;
    }

    blocks->size = ARENA_SIZE - sizeof(*blocks);
    blocks->next = NULL;
    blocks->free = 1;
    return 1;
}

static void split_block(block_t *block, size_t size) {
    block_t *remainder = (block_t *)((char *)(block + 1) + size);

    remainder->size = block->size - size - sizeof(*block);
    remainder->next = block->next;
    remainder->free = 1;

    block->next = remainder;
    block->size = size;
}

void *malloc(size_t size) {
    block_t *block;

    if (!size || !heap_init()) {
        return NULL;
    }

    size = align_size(size);
    for (block = blocks; block; block = block->next) {
        if (!block->free || block->size < size) {
            continue;
        }

        if (block->size >= size + sizeof(*block) + MALLOC_ALIGNMENT) {
            split_block(block, size);
        }

        block->free = 0;
        return block + 1;
    }

    return NULL;
}

void free(void *pointer) {
    block_t *block;

    if (!pointer) {
        return;
    }

    ((block_t *)pointer - 1)->free = 1;

    block = blocks;
    while (block && block->next) {
        if (block->free && block->next->free) {
            block->size += sizeof(*block) + block->next->size;
            block->next = block->next->next;
        } else {
            block = block->next;
        }
    }
}

void *calloc(size_t count, size_t size) {
    size_t total;
    void *pointer;

    if (size && count > (size_t)-1 / size) {
        return NULL;
    }

    total = count * size;
    pointer = malloc(total);
    if (pointer) {
        memset(pointer, 0, total);
    }
    return pointer;
}

void *realloc(void *pointer, size_t size) {
    block_t *old_block;
    void *new_pointer;

    if (!pointer) {
        return malloc(size);
    }
    if (!size) {
        free(pointer);
        return NULL;
    }

    old_block = (block_t *)pointer - 1;
    if (old_block->size >= size) {
        return pointer;
    }

    new_pointer = malloc(size);
    if (new_pointer) {
        memcpy(new_pointer, pointer, old_block->size);
        free(pointer);
    }
    return new_pointer;
}

void abort(void)
{
    _exit(1);
}

long strtol(const char *s, char **end, int base)
{
    while (*s == ' ') s++;

    int negative = (*s == '-');

    if (*s == '-' || *s == '+') s++;

    if (base == 0)
    {
        if (*s == '0')
        {
            if (s[1] == 'x' || s[1] == 'X')
            {
                base = 16;
                s += 2;
            } else
            {
                base = 8;
            }
        } else
        {
            base = 10;
        }
    }
    else if (
        base == 16 &&
        *s == '0' &&
        (
            s[1] == 'x' ||
            s[1] == 'X'
        )
    ) { s += 2; }

    long value = 0;

    while (*s)
    {
        int digit;

        if (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'z') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') digit = *s - 'A' + 10;
        else break;

        if (digit >= base) break;

        value = value * base + digit;
        s++;
    }

    if (end) *end = (char *)s;

    return negative ? -value : value;
}

static unsigned int rng_state = 123456789;

void srand(unsigned int seed)
{
    rng_state = seed;
}

int rand(void)
{
    rng_state = rng_state * 1103515245 + 12345;
    return (int)((rng_state >> 16) & 0x7FFF);
}

double strtod(const char *nptr, char **endptr)
{
    while (
    	*nptr == ' ' 	||
     	*nptr == '\t' 	||
      	*nptr == '\n' 	||
       	*nptr == '\r'
    ) {  nptr++; }

    int sign = 1;

    if (*nptr == '-')
    {
        sign = -1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }

    double value = 0.0;

    while (*nptr >= '0' && *nptr <= '9') {
        value = value * 10.0 + (*nptr - '0');
        nptr++;
    }

    if (*nptr == '.')
    {
        nptr++;
        double fraction = 1.0;

        while (*nptr >= '0' && *nptr <= '9')
        {
            fraction *= 10.0;
            value += (*nptr - '0') / fraction;
            nptr++;
        }
    }

    if (*nptr == 'e' || *nptr == 'E')
    {
        nptr++;
        int exp_sign = 1;

        if (*nptr == '-')
        {
            exp_sign = -1;
            nptr++;
        } else if (*nptr == '+')
        {
            nptr++;
        }

        int exp = 0;
        while (*nptr >= '0' && *nptr <= '9')
        {
            exp = exp * 10 + (*nptr - '0');
            nptr++;
        }

        double multiplier = 1.0;
        for (int i = 0; i < exp; i++)
        {
            multiplier *= 10.0;
        }

        if (exp_sign == -1)
        {
            value /= multiplier;
        } else
        {
            value *= multiplier;
        }
    }

    if (endptr)
    {
        *endptr = (char *)nptr;
    }

    return sign * value;
}

double atof(const char *nptr)
{
    return strtod(nptr, NULL);
}

int atoi(const char *s) { return (int)strtol(s, NULL, 10); }
long atol(const char *s)  { return strtol(s, NULL, 10); }
int abs (int x) { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }

char *getenv(const char *name) {
    (void)name;
    return NULL;
}

int system(const char *command) {
    (void)command;
    return -1;
}

void exit(int status) {
    _exit(status);
}

char *strdup(const char *source) {
    size_t length = strlen(source) + 1;
    char *copy = malloc(length);

    if (copy) {
        memcpy(copy, source, length);
    }
    return copy;
}
