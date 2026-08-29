/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: openfile.c
 *
 */
#include "openfile.h"
#include <kernel/devices/device_init.h>
#include <kernel/mem/meminclude.h>
#include <kernel/screen/lib/string.h>
#include <kernel/arch/hal/irqflags.h>

static open_file_t table[OPEN_FILE_MAX];

void openfile_init(void)
{
    memset(table, 0, sizeof(table));
}

int openfile_alloc(vfs_node_t *node, void *device_handle)
{
    irq_state_t st = irq_save();

    for (int i = 0; i < OPEN_FILE_MAX; i++)
    {
        if (!table[i].used)
        {
            table[i].node = node;
            table[i].offset = 0;
            table[i].device_handle = device_handle;
            table[i].refcount = 1;
            table[i].used = 1;

            irq_restore(st);
            return i;
        }
    }

    irq_restore(st);

    return -1;
}

void openfile_ref(int idx)
{
    if (idx < 0 || idx >= OPEN_FILE_MAX) return;

    irq_state_t st = irq_save();
    if (table[idx].used) table[idx].refcount++;
    irq_restore(st);
}

void openfile_unref(int idx)
{
    if (idx < 0 || idx >= OPEN_FILE_MAX) return;

    irq_state_t st = irq_save();

    if (!table[idx].used)
    {
        irq_restore(st);
        return;
    }

    table[idx].refcount--;

    int should_close = (table[idx].refcount <= 0);
    vfs_node_t *node = table[idx].node;
    void *handle = table[idx].device_handle;

    if (should_close) table[idx].used = 0;
    irq_restore(st);

    if (
        should_close &&
        node &&
        node->type == VFS_DEVICE &&
        node->device &&
        node->device->close
    ){
        node->device->close(handle);
    }
}

open_file_t *openfile_get(int idx)
{
    if (idx < 0 || idx >= OPEN_FILE_MAX) return NULL;
    if (!table[idx].used) return NULL;
    return &table[idx];
}
