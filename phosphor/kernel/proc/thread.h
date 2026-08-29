/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: thread.h
 *
 */

#ifndef THREAD_H
#define THREAD_H

#include <types.h>

#define     THREAD_STACK_SIZE 16384
#define     THREAD_NAME_MAX      32

typedef enum
{
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_DEAD
} thread_state_t;

typedef void  (*thread_entry_t)(void *arg);

struct     proc;

typedef struct thread {
    u64    tid;
    char    name[THREAD_NAME_MAX];
    thread_state_t  state;

    u64    rsp;
    u8     *stack_base;
    u64    stack_size;

    u8 fpu_state[512] __attribute__((aligned(16)));

    int    is_user;
    u64    kstack_top;

    u64    fork_user_r15;
    u64    fork_user_r14;
    u64    fork_user_r13;
    u64    fork_user_r12;
    u64    fork_user_r11;
    u64    fork_user_r10;
    u64    fork_user_r9;
    u64    fork_user_r8;
    u64    fork_user_rbp;
    u64    fork_user_rdi;
    u64    fork_user_rsi;
    u64    fork_user_rdx;
    u64    fork_user_rcx;
    u64    fork_user_rbx;
    u64    fork_user_rax;
    u64    fork_user_rip;
    u64    fork_user_rflags;
    u64    fork_user_rsp;

    struct proc    *owner;
    struct thread    *proc_next;
    struct thread    *sched_next;
    struct thread    *wait_next;
    struct wait_queue *wait_queue;
} __attribute__((aligned(16))) thread_t;

void   thread_subsystem_init(void);
void   thread_destroy(thread_t *t);
u64    thread_alloc_tid(void);

thread_t *thread_create(struct proc *owner, const char *name, thread_entry_t entry, void *arg);
thread_t *thread_create_user(struct proc *owner, const char *name, thread_entry_t entry,void *arg, u64 user_stack_top);
thread_t *thread_get_current(void);

__attribute__((noreturn)) void thread_exit(void);


#endif
