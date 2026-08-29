/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: sys_fs.h
 *
 */

#ifndef SYS_FS_H
#define SYS_FS_H

#include <types.h>
#include <kernel/arch/x86_64/idt/idt.h>

#define K_O_RDONLY  0x0000
#define K_O_WRONLY  0x0001
#define K_O_RDWR    0x0002
#define K_O_CREAT   0x0040
#define K_O_TRUNC   0x0200

void sys_open(cpu_state_t *state);
void sys_close(cpu_state_t *state);
void sys_lseek(cpu_state_t *state);
void sys_getdents(cpu_state_t *state);
void sys_mkdir(cpu_state_t *state);
void sys_unlink(cpu_state_t *state);
void sys_ftruncate(cpu_state_t *state);
void sys_rename(cpu_state_t *state);

#endif
