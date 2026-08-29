/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: process.c
 *
 */

#include "process.h"
#include "scheduler.h"
#include "signal.h"
#include <kernel/mem/meminclude.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/fs/openfile.h>
#include <kernel/devices/device_init.h>
#include <kernel/screen/lib/string.h>
#include <kernel/screen/lib/print.h>
#include <kernel/communication/serial.h>
#include <kernel/user/fb.h>

#include <kernel/arch/x86_64/fpu/fpu.h>

extern void fork_child_return(void);

#define STACK_SIZE 8192

static proc_t *head;
//static proc_t *current;
static proc_t *proc_zombies;
static u64 next_pid;

void process_init(void) {
    head = NULL;
    //current = NULL;
    proc_zombies = NULL;
    next_pid =    1;
    thread_subsystem_init();

    log("[PROC]", "Process manager\n");
}

static void proc_close_fds(proc_t *p)
{
    if (!p) return;

    for (int fd = 0; fd < FD_MAX; fd++)
    {
        if (!p->fd_table[fd].used) continue;
        if (p->fd_table[fd].ofd >= 0) openfile_unref(p->fd_table[fd].ofd);

        vfs_node_t *node = p->fd_table[fd].node;
        if (
            node &&
            node->type == VFS_DEVICE &&
            node->device &&
            node->device->close
        )node->device->close(p->fd_table[fd].device_handle);

        p->fd_table[fd].used = 0;
        p->fd_table[fd].ofd  = -1;
        p->fd_table[fd].node = NULL;
        p->fd_table[fd].device_handle = NULL;
    }
}

static proc_t *proc_alloc(const char *name)
{
    proc_t *p = (proc_t *)kcalloc(1, sizeof(proc_t));
    if (!p) return NULL;
/*
    u64 stk = (u64)kmalloc(STACK_SIZE);
    if (!stk) {
        kfree((u64 *)p);
        return NULL;
    }*/

    p->pid = next_pid++;
    p->state = PROC_ALIVE;
    p->exit_code    = 0;
    p->threads = NULL;
    p->thread_count = 0;
    p->alive_count  = 0;
    #ifdef ALWAYS_CAPFB 1
    	p->capabilities = CAP_FRAMEBUFFER;
    #else
    	p->capabilities = 0;
    #endif
    p->next = head;

    p->fd_table[0].used = 1;
    p->fd_table[0].ofd = -1;

    p->fd_table[1].used = 1;
    p->fd_table[1].ofd = -1;

    p->fd_table[2].used = 1;
    p->fd_table[2].ofd = -1;

    for (int i = 3; i < FD_MAX; i++)
    {
        p->fd_table[i].used = 0;
        p->fd_table[i].ofd = -1;
    }

    signal_proc_init(p);

    int i =    0;
    if (name)
    {
        while (name[i] && i < 63)
        {
            p->name[i] = name[i];
            i++;
        }
    }

    p->name[i] =  '\0';

    head =    p;

    return p;
}

proc_t *process_create(const char *name)
{
    proc_t *p = proc_alloc(name);
    if (!p) return NULL;

    p->space = vmm_get_kernel_space(); // no extra pml4
    return p;
}

proc_t *process_create_user(const char *name, u64 initial_caps)
{
    proc_t *p = proc_alloc(name);
    if (!p) return NULL;

    #ifdef ALWAYS_CAPFB 1
    	p->capabilities = initial_caps | CAP_FRAMEBUFFER;
    #else
    	p->capabilities = initial_caps;
    #endif

    p->space = vmm_space_create();

    if (!p->space)
    {
        head = p->next;

        kfree((u64 *)p);
        return NULL;
    }
    return p;
}

void process_exit(proc_t *p, int exit_code)
{
    if (!p || p->state != PROC_ALIVE) return;

    u64 saved_flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(saved_flags) :: "memory");

    p->exit_code = exit_code;
    p->state     = PROC_ZOMBIE;

    proc_t *cur  = head, *prev   = NULL;

    while (cur)
    {
        if (cur == p)
        {
            if (prev) prev->next = cur->next;
            else head     = cur->next;
            break;
        }
        prev     = cur;
        cur      = cur->next;
    }

    p->next = proc_zombies;
    proc_zombies = p;

    __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
}

void process_reap_zombies(void)
{
    /*
    u64 saved_flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(saved_flags) :: "memory");

    proc_t *cur  = proc_zombies;
    proc_t *prev = NULL;

    while (cur)
    {
        proc_t *next  = cur->next;

        if (cur->thread_count == 0)
        {
            if (
            	cur->space &&
             	cur->space != vmm_get_kernel_space()
            ) vmm_space_destroy(cur->space);

            cur->state = PROC_DEAD;

            if (prev) prev->next = next;
            else proc_zombies = next;

            kfree((u64 *)cur);
        }
        else
        {
            prev = cur;
        }

        cur = next;
    }

    __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
    */
}

int process_waitpid(
    proc_t *parent,
    i64 target_pid,
    int *exit_code_out
){
    (void)parent;
    proc_t *cur       = proc_zombies;
    proc_t *prev      = NULL;

    while (cur)
    {
        int pid_match = (target_pid == -1)    || ((u64)target_pid == cur->pid);

        if (pid_match && cur->thread_count == 0)
        {
            if (exit_code_out) *exit_code_out  = cur->exit_code;

            // unlink zombie lst
            if (prev)prev->next = cur->next;
            else proc_zombies = cur->next;
            proc_close_fds(cur);

            // free
            if (cur->space && cur->space != vmm_get_kernel_space())
                vmm_space_destroy(cur->space);

            cur->state     = PROC_DEAD;
            kfree((u64 *)cur);
            return 0;
        }

        prev   = cur;
        cur    = cur->next;
    }

    return -1;
}

void process_destroy(proc_t *p)
{
    if (!p) return;
    proc_close_fds(p);

    thread_t *t = p->threads;
    while (t) {
        thread_t *next = t->proc_next;
        thread_destroy(t);
        t = next;
    }
    if (
        p->space && p->space != vmm_get_kernel_space()
    )vmm_space_destroy(p->space);

    proc_t *cur = head, *prev = NULL;

    while (cur)
    {
        if (cur == p)
        {
            if (prev) prev->next = cur->next;
            else head = cur->next;

            //if (current == p) current = NULL;

            //kfree((u64 *)p->stack_base);
            kfree((u64 *)p);
            return;
        }

        prev    = cur;
        cur     = cur->next;
    }
}

proc_t *process_get_current(void)
{
    thread_t *t =    thread_get_current();

    return t ? t->owner  : NULL;
}
proc_t *process_find_by_pid(u64 pid)
{
    for (proc_t *cur = head; cur; cur = cur->next)
    {
        if (cur->pid == pid) return cur;
    }

    return NULL;
}

proc_t *process_fork(cpu_state_t *parent_state)
{
    proc_t   *parent = process_get_current();
    thread_t *parent_thread = thread_get_current();

    #if DEBUGINFO == 1
        printf(
            "[FORK] parent rip=0x%llx rsp=0x%llx rflags=0x%llx\n",
            parent_state->rip,
            parent_state->rsp,
            parent_state->rflags
        );
    #endif

    if (!parent || !parent_thread) return NULL;

    proc_t *child = proc_alloc(parent->name);
    if (!child) return NULL;

    child->capabilities = parent->capabilities;
    memcpy(child->fd_table, parent->fd_table, sizeof(parent->fd_table));

    for (int i = 0; i < FD_MAX; i++)
    {
        if (
             child->fd_table[i].used &&
             child->fd_table[i].ofd >= 0
        ) openfile_ref(child->fd_table[i].ofd);
    }

    signal_on_fork(child, parent);

    child->space = vmm_clone_space(parent->space);
    if (!child->space)
    {
        proc_t *cur = head, *prev = NULL;
        while (cur) {
            if (cur == child) {
                if (prev) prev->next = cur->next;
                else      head       = cur->next;
                break;
            }
            prev = cur; cur = cur->next;
        }
        kfree((u64 *)child);
        return NULL;
    }

    thread_t *ct = (thread_t *)kcalloc(1, sizeof(thread_t));
    if (!ct)
    {
        vmm_space_destroy(child->space);
        proc_t *cur = head, *prev = NULL;
        while (cur) {
            if (cur == child) {
                if (prev) prev->next = cur->next;
                else      head       = cur->next;
                break;
            }
            prev = cur; cur = cur->next;
        }
        kfree((u64 *)child);
        return NULL;
    }

    fpu_init_state(ct->fpu_state);

    u8 *kstack = (u8 *)kmalloc(THREAD_STACK_SIZE);
    if (!kstack)
    {
        kfree((u64 *)ct);
        vmm_space_destroy(child->space);
        proc_t *cur = head, *prev = NULL;
        while (cur)
        {
            if (cur == child)
            {
                if (prev) prev->next = cur->next;
                else head = cur->next;
                break;
            }
            prev = cur; cur = cur->next;
        }
        kfree((u64 *)child);
        return NULL;
    }

    int i = 0;
    while (parent_thread->name[i] && i < THREAD_NAME_MAX - 1) {
        ct->name[i] = parent_thread->name[i];
        i++;
    }
    ct->name[i]    = '\0';
    ct->tid        = thread_alloc_tid();
    ct->state      = THREAD_READY;
    ct->stack_base = kstack;
    ct->stack_size = THREAD_STACK_SIZE;
    ct->is_user    = 1;
    ct->kstack_top = (u64)(kstack + THREAD_STACK_SIZE);
    ct->owner      = child;
    ct->proc_next  = NULL;
    ct->sched_next = NULL;

    ct->fork_user_r15    = parent_state->r15;
    ct->fork_user_r14    = parent_state->r14;
    ct->fork_user_r13    = parent_state->r13;
    ct->fork_user_r12    = parent_state->r12;
    ct->fork_user_r11    = parent_state->r11;
    ct->fork_user_r10    = parent_state->r10;
    ct->fork_user_r9     = parent_state->r9;
    ct->fork_user_r8     = parent_state->r8;
    ct->fork_user_rbp    = parent_state->rbp;
    ct->fork_user_rdi    = parent_state->rdi;
    ct->fork_user_rsi    = parent_state->rsi;
    ct->fork_user_rdx    = parent_state->rdx;
    ct->fork_user_rcx    = parent_state->rcx;
    ct->fork_user_rbx    = parent_state->rbx;
    ct->fork_user_rax    = 0;
    ct->fork_user_rip    = parent_state->rip;
    ct->fork_user_rflags = parent_state->rflags;
    ct->fork_user_rsp    = parent_state->rsp;

    u64 *sp = (u64 *)(kstack + THREAD_STACK_SIZE);

    *(--sp) = (u64)fork_child_return;
    *(--sp) = 0x202;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = (u64)ct;

    ct->rsp = (u64)sp;

    ct->proc_next   = child->threads;
    child->threads  = ct;
    child->thread_count++;
    child->alive_count++;

    sched_add(ct);

    #if DEBUGINFO == 1
        printf(
            "[FORK] child rip=0x%llx rsp=0x%llx rflags=0x%llx\n",
            ct->fork_user_rip,
            ct->fork_user_rsp,
            ct->fork_user_rflags
        );
    #endif

    return child;
}

int process_has_cap(proc_t *p, u64 cap)
{
    if (!p) return 0;
    return (p->capabilities & cap) == cap;
}

void process_grant_cap(proc_t *p, u64 cap)
{
    if (!p) return;
    p->capabilities |= cap;
}

void process_revoke_cap(proc_t *p, u64 cap)
{
    if (!p) return;
    p->capabilities &= ~cap;
}
