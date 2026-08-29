/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: openfile.h
 *
 */

#ifndef OPENFILE_H
#define OPENFILE_H

#include <types.h>
#include <kernel/fs/vfs/vfs.h>

#define OPEN_FILE_MAX  256

typedef struct
{
    vfs_node_t *node;
    u64 offset;

    void *device_handle;
    int refcount;
    int used;
} open_file_t;

void openfile_init(void);
void openfile_ref(int idx);
void openfile_unref(int idx); // closes device on last ref
int openfile_alloc(vfs_node_t *node, void *device_handle); // refcount= 1
open_file_t *openfile_get(int idx);

#endif
