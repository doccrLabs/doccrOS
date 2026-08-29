/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: syscall.h
 *
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include <types.h>
#include "../idt/idt.h"

#define SYS_READ            0
#define SYS_WRITE           1
#define SYS_OPEN            2
#define SYS_CLOSE           3
#define SYS_IOCTL          16
#define SYS_LSEEK           8
#define SYS_MMAP            9
#define SYS_MUNMAP         11
#define SYS_BRK            12
#define SYS_DUP2           33
#define SYS_SIGNAL         34
#define SYS_SIGRETURN      35
#define SYS_SET_SIGTRAMP   36
#define SYS_KILL           62
#define SYS_FORK           57
#define SYS_EXECVE         59
#define SYS_UNAME          63
#define SYS_EXIT           60
#define SYS_GETPID         39
#define SYS_YIELD          24
#define SYS_WAITPID        61
#define SYS_GETDENTS       78
#define SYS_FTRUNCATE      77
#define SYS_RENAME         82
#define SYS_MKDIR          83
#define SYS_UNLINK         87
#define SYS_GETUID        102
#define SYS_GETGID        104
#define SYS_REBOOT        169
#define SYS_CLOCK_GETTIME 228
#define SYS_EVENTFD       290
#define SYS_EVENTFD_OPEN  291
#define SYS_IPC_CREATE    292
#define SYS_IPC_OPEN      293
#define SYS_IPC_SEND      294
#define SYS_IPC_RECV      295
#define SYS_IPC_EVENTFD   296
#define SYS_SPAWN         400

#define MSR_EFER       0xC0000080
#define MSR_STAR       0xC0000081
#define MSR_LSTAR      0xC0000082
#define MSR_SFMASK     0xC0000084

#define EFER_SCE (1ULL << 0)

extern void isr128(void);
extern void syscall_entry(void);

extern u64 syscall_scratch[2];

void syscall_install(void);
void syscall_update_kstack(u64 kstack_top);
void syscall_dispatch(cpu_state_t *state);

#endif
