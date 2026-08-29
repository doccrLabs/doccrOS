/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: syscall.c
 *
 */

#include "syscall.h"

#include <kernel/proc/signal.h>

#include "sys_io.h"
#include "sys_fs.h"
#include "sys_process.h"
#include "sys_mem.h"
#include "sys_ioctl.h"
#include "sys_power.h"
#include "sys_eventfd.h"
#include "sys_ipc.h"
#include "sys_uname.h"


#include <kernel/arch/x86_64/idt/idt.h>
#include <kernel/arch/x86_64/gdt/gdt.h>
#include <kernel/screen/lib/print.h>
#include <kernel/arch/hal/timer.h>

#include <types.h>

u64 syscall_scratch[2];

int user_ptr_ok(u64 ptr)
{
    return ptr != 0 && ptr <= 0x00007FFFFFFFFFFFULL;
}

static inline void wrmsr(u32 msr, u64 val)
{
    __asm__ volatile(
        "wrmsr"
        :
        : "c"(msr), "a"((u32)(val & 0xFFFFFFFF)), "d"((u32)(val >> 32))
    );
}

static inline u64 rdmsr(u32 msr)
{
    u32 lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((u64)hi << 32) | lo;
}

static void syscall_enable(void)
{
    u64 efer = rdmsr(MSR_EFER);
    wrmsr(MSR_EFER, efer | EFER_SCE);

    /*
     * SYSCALL loads kernel CS from STAR[47:32]. SYSRET derives user SS
     * as STAR[63:48] + 8 and user CS as STAR[63:48] + 16. Keep RPL=3
     * in the base selector so SYSRET produces SS=0x1b and CS=0x23; an
     * SS of 0x18 is rejected by iretq when an IRQ returns to CPL 3.
     */
    const u64 user_sysret_base = (USER_DATA_SELECTOR - 8) | 3;
    u64 star = ((u64)KERNEL_CODE_SELECTOR << 32) |
               (user_sysret_base << 48);
    wrmsr(MSR_STAR, star);

    wrmsr(MSR_LSTAR, (u64)syscall_entry);

    wrmsr(MSR_SFMASK, (1 << 9));
}

void syscall_install(void)
{
    idt_set_gate(128, (u64)isr128, IDT_FLAG_PRESENT | IDT_FLAG_RING3 | IDT_FLAG_GATE_INT);
    syscall_enable();
    log("[SYSCALL]", "INT 128 + SYSCALL/SYSRET enabled\n");
}

void syscall_update_kstack(u64 kstack_top)
{
    syscall_scratch[1] = kstack_top;
}

void syscall_dispatch(cpu_state_t *state)
{
    switch (state->rax)
    {
        case SYS_READ:           sys_read(state);               break;
        case SYS_WRITE:          sys_write(state);              break;
        case SYS_OPEN:           sys_open(state);               break;
        case SYS_CLOSE:          sys_close(state);              break;
        case SYS_IOCTL:          sys_ioctl(state);              break;
        case SYS_LSEEK:          sys_lseek(state);              break;
        case SYS_MMAP:           sys_mmap(state);               break;
        case SYS_MUNMAP:         sys_munmap(state);             break;
        case SYS_BRK:            sys_brk(state);                break;
        case SYS_FORK:           sys_fork(state);               break;
        case SYS_DUP2:           sys_dup2(state);               break;
        case SYS_EXECVE:         sys_execve(state);             break;
        case SYS_SPAWN:          sys_spawn(state);              break;
        case SYS_EXIT:           sys_exit(state);               break;
        case SYS_UNAME:          sys_uname(state);              break;
        case SYS_YIELD:          sys_yield(state);              break;
        case SYS_GETPID:         sys_getpid(state);             break;
        case SYS_WAITPID:        sys_waitpid(state);            break;
        case SYS_GETDENTS:       sys_getdents(state);           break;
        case SYS_FTRUNCATE:      sys_ftruncate(state);          break;
        case SYS_RENAME:         sys_rename(state);             break;
        case SYS_MKDIR:          sys_mkdir(state);              break;
        case SYS_UNLINK:         sys_unlink(state);             break;
        case SYS_GETUID:         sys_getuid(state);             break;
        case SYS_GETGID:         sys_getgid(state);             break;
        case SYS_REBOOT:         sys_reboot(state);             break;
        case SYS_EVENTFD:        sys_eventfd(state);            break;
        case SYS_EVENTFD_OPEN:   sys_eventfd_open(state);       break;
        case SYS_IPC_CREATE:     sys_ipc_create(state);         break;
        case SYS_IPC_OPEN:       sys_ipc_open(state);           break;
        case SYS_IPC_SEND:       sys_ipc_send(state);           break;
        case SYS_IPC_RECV:       sys_ipc_recv(state);           break;
        case SYS_IPC_EVENTFD:    sys_ipc_get_eventfd(state);    break;
        case SYS_CLOCK_GETTIME:
        {
            struct user_timespec { i64 tv_sec; i64 tv_nsec; };
            struct user_timespec *ts = (void *)state->rsi;
            if (!ts || (u64)ts > 0x00007fffffffffffULL) { state->rax = (u64)-1; break; }
            u64 ms = timer_get_ticks();
            ts->tv_sec = (i64)(ms / 1000);
            ts->tv_nsec = (i64)((ms % 1000) * 1000000);
            state->rax = 0;
            break;
        }
        case SYS_KILL:
            state->rax = (u64)sys_kill_impl(state->rdi, (int)state->rsi);
            break;
        case SYS_SIGNAL:
        {
            int ok;
            u64 old = sys_signal_impl((int)state->rdi, state->rsi, &ok);
            state->rax = ok ? old : (u64)-1;
            break;
        }
        case SYS_SIGRETURN:  // all from proc/signal.c btw
            sys_sigreturn_impl(state);
            break;
        case SYS_SET_SIGTRAMP:
            sys_set_sigtramp_impl(state->rdi);
            state->rax = 0;
            break;

        default:
            state->rax = (u64)-1;
            break;
    }
    signal_check_and_deliver(state);
}
