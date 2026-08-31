/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: thread.c
 *
 */

#include "thread.h"
#include "process.h"
#include "scheduler.h"
#include "wait.h"
#include <kernel/mem/meminclude.h>
#include <kernel/arch/hal/halt.h>
#include <kernel/screen/lib/string.h>
#include <kernel/arch/x86_64/fpu/fpu.h>

extern void thread_trampoline(void);
extern void user_thread_trampoline(void);

/*
 * A new context must not enable interrupts in context_switch's popfq/ret
 * window. Kernel threads enable them in thread_trampoline; user threads get
 * IF from the iretq frame built by arch_enter_usermode.
 */
#define INITIAL_RFLAGS 0x2

static u64 next_tid = 1;

void thread_subsystem_init(void) {
    next_tid     = 1;
}

u64 thread_alloc_tid(void)
{
    return next_tid++;
}

thread_t *thread_create(proc_t *owner, const char *name, thread_entry_t entry, void *arg)
{
    if (!entry) return NULL; // cant run nothing, sorry

    thread_t *t = (thread_t *)kcalloc(1, sizeof(thread_t));
    if (!t) return NULL;
    fpu_init_state(t->fpu_state);

    u8 *stack = (u8 *)kmalloc(THREAD_STACK_SIZE);

    if (!stack) {
        kfree((u64 *)t);
        return NULL;
    }
    *(u64 *)stack = STACK_CANARY;

    t->tid            = next_tid++;
    t->state          = THREAD_READY;
    t->stack_base     = stack;
    t->stack_size     = THREAD_STACK_SIZE;
    t->owner          = owner;
    t->proc_next      = NULL;
    t->sched_next     = NULL;
    t->is_user = 0;
    t->kstack_top = 0;

    int i = 0;
    if (name)
    {
        while (
            name[i] && i < THREAD_NAME_MAX - 1
        ){
            t->name[i] = name[i];
            i++;
        }
    }
    t->name[i]  =  '\0';

    u64 *sp = (u64 *)(stack + THREAD_STACK_SIZE);


    *(--sp)     = (u64)thread_trampoline;
    *(--sp)     = INITIAL_RFLAGS;
    *(--sp)     = 0;
    *(--sp)     = 0;
    *(--sp)     = (u64)entry;
    *(--sp)     = (u64)arg;
    *(--sp)     = 0;
    *(--sp)     = 0;

    t->rsp     = (u64)sp;

    if (owner) {
        t->proc_next   = owner->threads;
        owner->threads     = t;

        owner->thread_count++;
        owner->alive_count++;
    }

    sched_add(t);
    return t;
}

thread_t *thread_create_user(
    proc_t *owner,
    const char *name,
    thread_entry_t entry,
    void *arg,
    u64 user_stack_top
) {
    if (!entry || !owner)  return NULL;

    thread_t *t = (thread_t *)kcalloc(1, sizeof(thread_t));
    if (!t) return NULL;
    fpu_init_state(t->fpu_state);

    u8 *kstack = (u8 *)kmalloc(THREAD_STACK_SIZE);
    if (!kstack)
    {
        kfree((u64 *)t);
        return NULL;
    }
    *(u64 *)kstack = STACK_CANARY;

    t->tid            = next_tid++;
    t->state          = THREAD_READY;
    t->stack_base     = kstack;
    t->stack_size     = THREAD_STACK_SIZE;
    t->owner          = owner;
    t->proc_next      = NULL;
    t->sched_next     = NULL;
    t->is_user = 1;
    t->kstack_top = (u64)(kstack + THREAD_STACK_SIZE);

    int i = 0;
    if (name)
    {
        while (
            name[i] && i < THREAD_NAME_MAX - 1
        ) {
            t->name[i] = name[i];
            i++;
        }
    }
    t->name[i]  =  '\0';

    u64 *sp = (u64 *)(kstack + THREAD_STACK_SIZE);

    *(--sp)     = (u64)user_thread_trampoline;
    *(--sp)     = INITIAL_RFLAGS;
    *(--sp)     = 0;
    *(--sp)     = 0;
    *(--sp)     = (u64)entry;
    *(--sp)     = (u64)arg;
    *(--sp)     = user_stack_top;
    *(--sp)     = 0;

    t->rsp     = (u64)sp;

    t->proc_next   = owner->threads;
    owner->threads     = t;
    owner->thread_count++;
    owner->alive_count++;

    sched_add(t);
    return t;
}

void thread_destroy(thread_t *t)
{
    if (!t) return;

    u64 saved_flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(saved_flags) :: "memory");

    sched_remove(t);
    wait_queue_remove(t);

    if (t->owner)
    {
        thread_t **cur = &t->owner->threads;
        while (*cur)
        {
            if (*cur == t)
            {
                *cur =     t->proc_next;
                t->owner-> thread_count--;
                break;
            }
            cur = &(*cur)-> proc_next;
        }
    }

    __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");

    if (t->stack_base) kfree((u64 *)t->stack_base);

    kfree((u64 *)t);
}
__attribute__((noreturn)) void thread_exit(void)
{
    thread_t *self  = thread_get_current();
    self->state     = THREAD_DEAD;

    if (self->owner)
    {
        self->owner->alive_count--;
        if (
            self->owner->alive_count <= 0 &&
            self->owner->state       == PROC_ALIVE
        ) process_exit(self->owner, 0); // if last thread is dead, proces is also dead
    }

    sched_yield();

    for (;;)
    {
        halt();
    }
}

thread_t *thread_get_current(void)
{
    return sched_current();
}
