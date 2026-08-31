/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: signal.c
 *
 */

#include "signal.h"
#include "scheduler.h"

#include <kernel/mem/meminclude.h>
#include <kernel/mem/paging/paging.h>
#include <kernel/mem/vmm/vmm.h>
#include <kernel/screen/lib/string.h>
#include <kernel/screen/lib/print.h>
#include <kernel/arch/hal/irqflags.h>
#include <kernel/arch/x86_64/user/ptr.h>
#include <kernel/arch/x86_64/exceptions/panic.h>
#include <kernel/communication/serial.h>


static int valid_sig(int sig)
{
    return sig == SIGINT || sig == SIGTERM;
}

typedef struct
{
    u64 magic;

    u64 r15;
    u64 r14;
    u64 r13;
    u64 r12;
    u64 r11;
    u64 r10;
    u64 r9;
    u64 r8;

    u64 rbp;
    u64 rdi;
    u64 rsi;
    u64 rdx;
    u64 rcx;
    u64 rbx;
    u64 rax;

    u64 rip;
    u64 rflags;
    u64 rsp;

    int signum;
    int pad;

} sigframe_t;


void signal_proc_init(proc_t *p)
{
    p->pending_signals = 0;
    p->in_signal = 0;
    p->sig_trampoline = 0;

    for (int i = 0; i < MAX_SIGNALS; i++) p->sig_handlers[i] = SIG_DFL;
}


void signal_on_fork(proc_t *child, proc_t *parent)
{
    child->pending_signals = 0;
    child->in_signal = 0;
    child->sig_trampoline = parent->sig_trampoline;

    for (int i = 0; i < MAX_SIGNALS; i++) child->sig_handlers[i] = parent->sig_handlers[i];
}


void signal_on_exec(proc_t *p)
{
    for (int i = 0; i < MAX_SIGNALS; i++)
    {
        if (
            p->sig_handlers[i] != SIG_DFL &&
            p->sig_handlers[i] != SIG_IGN
        ) {
            p->sig_handlers[i] = SIG_DFL;
        }
    }

    p->pending_signals = 0;
    p->in_signal = 0;
    p->sig_trampoline = 0;
}


int sys_kill_impl(u64 pid, int sig)
{
    if ((i64)pid <= 0) return -1;
    if (!valid_sig(sig)) return -1;

    proc_t *target = process_find_by_pid(pid);

    if (!target || target->state != PROC_ALIVE) return -1;

    irq_state_t st = irq_save();

    target->pending_signals |= (1ULL << sig);
    irq_restore(st);

    return 0;
}


u64 sys_signal_impl(int sig, u64 handler, int *out_ok)
{
    proc_t *p = process_get_current();

    *out_ok = 0;

    if (!p || !valid_sig(sig)) return SIG_DFL;

    u64 old = p->sig_handlers[sig];

    p->sig_handlers[sig] = handler;
    *out_ok = 1;

    return old;
}


void sys_set_sigtramp_impl(u64 addr)
{
    proc_t *p = process_get_current();

    if (!p) return;
    if (addr == 0 || addr > 0x00007FFFFFFFFFFFULL) return;

    p->sig_trampoline = addr;
}

#define SIGFRAME_MAGIC 0x5347524D41455346ULL
static int copy_to_user(
    vmm_space_t *space,
    u64 uaddr,
    const void *ksrc,
    u64 len
)
{
    if (!user_range_ok(uaddr, len)) return -1;

    u64 hhdm = paging_get_hhdm_offset();
    u64 va = uaddr;
    u64 remaining = len;

    const u8 *src = (const u8 *)ksrc;

    while (remaining > 0)
    {
        u64 page_va = va & ~0xFFFULL;
        u64 page_off = va - page_va;
        u64 phys = vmm_space_get_phys(space, page_va);
        u8 *dest = (u8 *)(phys + hhdm + page_off);
        u64 chunk = 4096 - page_off;

        if (!phys) return -1;
        if (chunk > remaining) chunk = remaining;

        memcpy(dest, src, chunk);

        va += chunk;
        src += chunk;
        remaining -= chunk;
    }

    return 0;
}


static int copy_from_user(
    vmm_space_t *space,
    u64 uaddr,
    void *kdst,
    u64 len
)
{
    u64 hhdm = paging_get_hhdm_offset();
    u64 va = uaddr;
    u64 remaining = len;

    u8 *dst = (u8 *)kdst;

    while (remaining > 0)
    {
        u64 page_va = va & ~0xFFFULL;
        u64 page_off = va - page_va;
        u64 phys = vmm_space_get_phys(space, page_va);
        u8 *src = (u8 *)(phys + hhdm + page_off);
        u64 chunk = 4096 - page_off;

        if (chunk > remaining) chunk = remaining;

        memcpy(dst, src, chunk);

        va += chunk;
        dst += chunk;
        remaining -= chunk;
    }

    return 0;
}


__attribute__((noreturn))
static void terminate_current_for_signal(int sig)
{
    thread_t *self = thread_get_current();
    proc_t *p = self ? self->owner : NULL;

    #if DEBUGINFO
        printf(
            "[SIG] pid=%llu terminated by signal %d\n",
            p ? p->pid : 0,
            sig
        );
    #endif
    if (self)
    {
        self->state = THREAD_DEAD;

        if (p && p->alive_count > 0) p->alive_count--;
    }

    if (p && p->state == PROC_ALIVE) process_exit(p, 128 + sig);

    sched_yield();

    __asm__ volatile("sti");
    for (;;) __asm__ volatile("hlt");
}


void signal_check_and_deliver(cpu_state_t *state)
{
    proc_t *p = process_get_current();
    int sig = -1;

    if ((state->cs & 3) != 3) return;
    if (!p || p->state != PROC_ALIVE) return;
    if (p->pending_signals == 0) return;
    if (p->in_signal) return;


    for (int i = 1; i < MAX_SIGNALS; i++)
    {
        if (p->pending_signals & (1ULL << i))
        {
            sig = i;
            break;
        }
    }

    if (sig < 0) return;

    p->pending_signals &= ~(1ULL << (u64)sig);

    u64 h = p->sig_handlers[sig];
    sigframe_t frame;

    if (h == SIG_IGN) return;
    if (h == SIG_DFL) terminate_current_for_signal(sig);
    if (p->sig_trampoline == 0) terminate_current_for_signal(sig);

    frame.magic = SIGFRAME_MAGIC;

    frame.r15 = state->r15;
    frame.r14 = state->r14;
    frame.r13 = state->r13;
    frame.r12 = state->r12;
    frame.r11 = state->r11;
    frame.r10 = state->r10;
    frame.r9 = state->r9;
    frame.r8 = state->r8;

    frame.rbp = state->rbp;
    frame.rdi = state->rdi;
    frame.rsi = state->rsi;
    frame.rdx = state->rdx;
    frame.rcx = state->rcx;
    frame.rbx = state->rbx;
    frame.rax = state->rax;

    frame.rip = state->rip;
    frame.rflags = state->rflags;
    frame.rsp = state->rsp;

    frame.signum = sig;
    frame.pad = 0;

    u64 usp = state->rsp;

    usp -= 512;
    usp &= ~0xFULL;
    usp -= sizeof(sigframe_t);
    usp &= ~0xFULL;

    if (
        copy_to_user
        (
            p->space,
            usp,
            &frame,
            sizeof(frame)
        )
        != 0
    ) {
        terminate_current_for_signal(sig);
    }

    u64 handler_rsp = usp - 16;
    u64 tramp = p->sig_trampoline;
    u64 frame_addr = usp;

    if (
        copy_to_user
        (
            p->space,
            handler_rsp,
            &tramp,
            8
        )
        != 0
    ) {
        terminate_current_for_signal(sig);
    }

    if (
        copy_to_user
        (
            p->space,
            handler_rsp + 8,
            &frame_addr,
            8
        )
        != 0
    ){
        terminate_current_for_signal(sig);
    }

    p->in_signal = 1;

    state->rdi = (u64)sig;
    state->rip = h;
    state->rsp = handler_rsp;
    state->rflags &= ~(1ULL << 8);
}


void sys_sigreturn_impl(cpu_state_t *state)
{
    proc_t *p = process_get_current();

    if (!p || !p->in_signal)
    {
        state->rax = (u64)-1;
        return;
    }

    u64 frame_ptr = state->rdi;
    if (
        !user_range_ok(
            frame_ptr,
            sizeof(sigframe_t)
        )
    ) {
        state->rax = (u64)-1;
        return;
    }

    sigframe_t frame;

    if (
        copy_from_user
        (
            p->space,
            frame_ptr,
            &frame,
            sizeof(frame)
        )
        != 0
    ) {
        state->rax = (u64)-1;
        return;
    }

    if (frame.magic != SIGFRAME_MAGIC)
    {
        state->rax = (u64)-1;
        return;
    }

    state->r15 = frame.r15;
    state->r14 = frame.r14;
    state->r13 = frame.r13;
    state->r12 = frame.r12;
    state->r11 = frame.r11;
    state->r10 = frame.r10;
    state->r9 = frame.r9;
    state->r8 = frame.r8;

    state->rbp = frame.rbp;
    state->rdi = frame.rdi;
    state->rsi = frame.rsi;
    state->rdx = frame.rdx;
    state->rcx = frame.rcx;
    state->rbx = frame.rbx;
    state->rax = frame.rax;

    state->rip = frame.rip;
    state->rflags = frame.rflags;
    state->rsp = frame.rsp;

    p->in_signal = 0;
}