/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: procfs.h
 *
 */

#ifndef PROCFS_H
#define PROCFS_H

#include <types.h>

typedef int (*procfs_gen_t) (char *buf, u64 size);

#define PROCFS_MAX_ENTRIES 32
int procfs_register(const char *name, procfs_gen_t generator);
void procfs_init(void);
void procfs_dir_init(void);

#endif
