/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: sys_io.c
 *
 */

#include "sys_io.h"
#include "ptr.h"
#include "../../../fs/openfile.h"
#include <kernel/screen/lib/print.h>
#include <kernel/devices/input/kbd.h>
#include <kernel/communication/serial.h>
#include <kernel/proc/process.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/fs/tmpfs/tmpfs.h>
#include <kernel/devices/device_init.h>
#include <kernel/devices/vt/vt.h>
#include <kernel/mem/mem.h>

void sys_read(cpu_state_t *state)
{
    u64 fd  = state->rdi;
    char *buf = (char *)state->rsi;
    u64 len = state->rdx;

    if (!user_ptr_ok((u64)buf))
    {
        state->rax = (u64)-1;
        return;
    }

    if (fd >= FD_MAX)
    {
        state->rax = (u64)-1;
        return;
    }

    proc_t *p = process_get_current();
    if (!p    || !p->fd_table[fd].used)
    {
        state->rax = (u64)-1;
        return;
    }

    int ofd = p->fd_table[fd].ofd;

    if (ofd < 0)
    {
        if (fd == 0)
        {
            state->rax = kbd_read_events(buf, len);
            return;
        }

        state->rax = (u64)-1;
        return;
    }

    open_file_t *of = openfile_get(ofd);
    if (!of)
    {
        state->rax = (u64)-1;
        return;
    }

    if (of->node && of->node->type == VFS_DEVICE)
    {
        if (!of->node->device || !of->node->device->read)
        {
            state->rax = (u64)-1;
            return;
        }

        state->rax = (u64)of->node->device->read(
            of->device_handle,
            buf,
            len
        );

        return;
    }

    if (!of->node || of->node->type != VFS_FILE)
    {
        state->rax  = (u64)-1;
        return;
    }

    if (of->offset >= of->node->size)
    {
        state->rax = 0;
        return;
    }

    u64 remaining = of->node->size - of->offset;
    u64 to_copy = (len < remaining) ? len : remaining;

    memcpy(
        buf,
        of->node->data + of->offset,
        to_copy
    );

    of->offset += to_copy;

    state->rax = to_copy;
}

void sys_write(cpu_state_t *state)
{
    u64 fd        = state->rdi;
    const char *buf = (const char *)state->rsi;
    u64 len       = state->rdx;
    proc_t *p     = process_get_current();

    if (!user_ptr_ok((u64)buf))
    {
        state->rax = (u64)-1;
        return;
    }

    if (fd >= FD_MAX)
    {
        state->rax = (u64)-1;
        return;
    }

    if (!p || !p->fd_table[fd].used)
    {
        state->rax = (u64)-1;
        return;
    }

    int ofd = p->fd_table[fd].ofd;

    if (ofd < 0)
    {
        if (fd == 1 || fd == 2)
        {
            int owns_framebuffer = process_has_cap(
                p,
                CAP_FRAMEBUFFER
            );

            for (u64 i = 0; i < len; i++)
            {
                if (vt_screen_enabled())
                {
                    putchar(buf[i], white());
                }

                serial_putchar(buf[i]);
            }

            state->rax = len;
            return;
        }

        state->rax = (u64)-1;
        return;
    }

    open_file_t *of = openfile_get(ofd);
    if (!of)
    {
        state->rax = (u64)-1;
        return;
    }

    if (of->node && of->node->type == VFS_DEVICE)
    {
        if (!of->node->device || !of->node->device->write)
        {
            state->rax = (u64)-1;
            return;
        }

        state->rax = (u64)of->node->device->write(
            of->device_handle,
            buf,
            len
        );

        return;
    }

    if (!of->node || of->node->type != VFS_FILE)
    {
        state->rax  = (u64)-1;
        return;
    }

    int written = tmpfs_write(
        of->node,
        buf,
        len,
        of->offset
    );

    if (written > 0) of->offset += (u64)written;

    state->rax = (written < 0) ? (u64)-1 : (u64)written;
}
