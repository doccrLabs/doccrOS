/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: sys_fs.c
 *
 */

#include "sys_fs.h"
#include "ptr.h"
#include "../../../fs/openfile.h"
#include <kernel/proc/process.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/fs/tmpfs/tmpfs.h>
#include <kernel/devices/device_init.h>

void sys_open(cpu_state_t *state)
{
    const char *path = (const char *)state->rdi;
    u64 flags        = state->rsi;

    if (!user_ptr_ok((u64)path))
    {
        state->rax = (u64)-1;
        return;
    }

    vfs_node_t *node = vfs_find(path);
    if (!node && (flags & K_O_CREAT))
    {
        node = tmpfs_create(path);
    }
    if (!node)
    {
        state->rax = (u64)-1;
        return;
    }

    if (node->type == VFS_FILE && (flags & (K_O_WRONLY | K_O_RDWR))) node->size = 0;

    void *handle = NULL;

    if (node->type == VFS_DEVICE && node->device && node->device->open)
    {
        handle = node->device->open(path);
        if (!handle)
        {
            state->rax = (u64)-1;
            return;
        }
    }

    int ofd = openfile_alloc(node, handle);
    if (ofd < 0)
    {
        if (node->type == VFS_DEVICE && handle && node->device->close)
            node->device->close(handle);

        state->rax = (u64)-1;
        return;
    }

    proc_t *p = process_get_current();
    if (!p)
    {
        openfile_unref(ofd);
        state->rax = (u64)-1;
        return;
    }

    for (int fd = 3; fd < FD_MAX; fd++)
    {
        if (!p->fd_table[fd].used)
        {
            void *handle = NULL;
            p->fd_table[fd].used = 1;
            p->fd_table[fd].ofd = ofd;

            state->rax = (u64)fd;
            return;
        }
    }

    openfile_unref(ofd);
    state->rax = (u64)-1;
}

void sys_close(cpu_state_t *state)
{
    u64 fd = state->rdi;

    if (fd < 3 || fd >= FD_MAX)
    {
        state->rax = (u64)-1;
        return;
    }

    proc_t *p = process_get_current();
    if (!p || !p->fd_table[fd].used)
    {
        state->rax = (u64)-1;
        return;
    }

    if (p->fd_table[fd].ofd >= 0) openfile_unref(p->fd_table[fd].ofd);

    p->fd_table[fd].used = 0;
    p->fd_table[fd].ofd = -1;

    state->rax = 0;
}

void sys_rename(cpu_state_t *state)
{
    const char *oldpath = (const char *)state->rdi;
    const char *newpath = (const char *)state->rsi;

    if (
        !user_ptr_ok((u64)oldpath) ||
        !user_ptr_ok((u64)newpath)
    ) {
        state->rax = (u64)-1;
        return;
    }

    int result = vfs_rename(oldpath, newpath);

    state->rax = (result == 0) ? 0 : (u64)-1;
}

void sys_lseek(cpu_state_t *state)
{
    u64 fd = state->rdi;
    i64 offset   = (i64)state->rsi;
    int whence   = (int)state->rdx;

    if (fd < 3   || fd >= FD_MAX)
    {
        state->rax   = (u64)-1;
        return;
    }

    proc_t *p    = process_get_current();
    if (!p ||    !p->fd_table[fd].used)
    {
        state->rax   = (u64)-1;
        return;
    }

    open_file_t *of = openfile_get(p->fd_table[fd].ofd);
    if (!of || !of->node)
    {
        state->rax = (u64)-1;
        return;
    }

    vfs_node_t *node = of->node;

    if (node->type != VFS_FILE)
    {
        state->rax = (u64)-1;
        return;
    }

    i64 new_off;

    if (whence == 0) new_off = offset;
    else if (whence == 1) new_off = (i64)of->offset + offset;
    else if (whence == 2) new_off = (i64)node->size + offset;
    else
    {
        state->rax = (u64)-1; // unknown?
        return;
    }

    if (new_off < 0)
    {
        state->rax = (u64)-1; // no seak vefore start
        return;
    }

    of->offset = (u64)new_off;
    state->rax = (u64)new_off;
}

typedef struct
{
    u64    d_ino;
    i64    d_off;
    u16    d_reclen;
    u8     d_type;
    char    d_name[256];
} linux_dirent64_t;

#define DT_REG    8
#define DT_DIR    4


void sys_getdents(cpu_state_t *state)
{
    u64 fd         = state->rdi;
    u8  *buf       = (u8 *)state->rsi;
    u64 buf_size   = state->rdx;

    if (!user_ptr_ok((u64)buf))
    {
        state->rax = (u64)-1;
        return;
    }

    if (fd < 3 ||  fd >= FD_MAX)
    {
        state->rax = (u64)-1;
        return;
    }

    proc_t *p = process_get_current();
    if (!p || !p->fd_table[fd].used)
    {
        state->rax = (u64)-1;
        return;
    }

    open_file_t *of = openfile_get(p->fd_table[fd].ofd);
    if (!of || !of->node)
    {
        state->rax = (u64)-1; // not a dir
        return;
    }

    vfs_node_t *dir = of->node;

    if (dir->type != VFS_DIRECTORY)
    {
        state->rax = (u64)-1;
        return;
    }

    u64 written = 0;
    int start   = (int)of->offset;

    for (
        int i = start;
        i < dir-> child_count;
        i++
    ) {
        vfs_node_t *child = dir->children[i];
        if (!child) continue;

        // figures out how long the name actually is
        int name_len = 0;
        while (
            child->name[name_len] &&
            name_len < VFS_NAME_MAX - 1
        )name_len++;

        u64 entry_size = sizeof(linux_dirent64_t);

        if (written + entry_size > buf_size)break;

        linux_dirent64_t *entry  = (linux_dirent64_t *)(buf + written);
        entry->d_ino    = (u64)(i + 1); // fake inode, whatever
        entry->d_off    = (i64)(written + entry_size);
        entry->d_reclen = (u16)entry_size;
        entry->d_type   = (child->type == VFS_DIRECTORY) ? DT_DIR : DT_REG;

        int j = 0;
        while (child->name[j] && j < 255)
        {
            entry->d_name[j] = child->name[j];
            j++;
        }
        entry->d_name[j] = '\0';

        written += entry_size;
        of->offset = (u64)(i + 1);
    }

    state->rax = written; // bytes into buffer
}

void sys_mkdir(cpu_state_t *state)
{
    const char *path = (const char *)state->rdi;

    if (!user_ptr_ok((u64)path))
    {
        state->rax = (u64)-1;
        return;
    }

    vfs_node_t *node = vfs_mkdir(path);
    if (!node)
    {
        state->rax = (u64)-1;
        return;
    }

    state->rax = 0;
}
void sys_unlink(cpu_state_t *state)
{
    const char *path = (const char *)state->rdi;

    if (!user_ptr_ok((u64)path))
    {
        state->rax   = (u64)-1;
        return;
    }

    int result = vfs_remove(path);
    state->rax = (result == 0) ? 0 : (u64)-1;
}

void sys_ftruncate(cpu_state_t *state)
{
    u64 fd = state->rdi;
    u64 size = state->rsi;

    proc_t *p = process_get_current();

    if (fd < 3 || fd >= FD_MAX || !p || !p->fd_table[fd].used)
    {
        state->rax = (u64)-1;
        return;
    }

    open_file_t *of = openfile_get(p->fd_table[fd].ofd);
    if (!of || !of->node)
    {
        state->rax = (u64)-1;
        return;
    }

    state->rax = (tmpfs_truncate(of->node, size) < 0) ? (u64)-1 : 0;
}
