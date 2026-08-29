/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: tmpfs.c
 *
 */

#include "tmpfs.h"
#include <kernel/mem/meminclude.h>

vfs_node_t *tmpfs_create(const char *path)
{
    if (!path) return NULL;

    char dirpath[VFS_MAX_PATH];
    char fname[VFS_NAME_MAX];
    vfs_split_path(path, dirpath, fname);

    if (fname[0] == '\0') return NULL;

    vfs_node_t *dir = vfs_find(dirpath);
    if (!dir) dir = vfs_mkdir(dirpath); // THEN BUILD THAT DIR!!!
    if (!dir) return NULL;

    return vfs_create_node(dir, fname, VFS_FILE);
}

int tmpfs_write(vfs_node_t *node, const void *buf, u64 size, u64 offset)
{
    if (!node || node->type != VFS_FILE || !buf) return -1;
    if (offset > VFS_MAX_FILE_SIZE || size > VFS_MAX_FILE_SIZE - offset) return -1;

    if (node->borrowed)
    {
        node->data = NULL;
        node->capacity = 0;
        node->borrowed = 0;
    }

    u64 needed = offset + size;
    if (!node->data || node->capacity < needed)
    {
        u8 *newbuf = (u8 *)kmalloc(needed);
        if (!newbuf) return -1;
        if (node->data)
        {
            u64 keep = node->size < offset ? node->size : offset;
            memcpy(newbuf, node->data, keep);
            kfree((u64 *)node->data);
        }

        node->data = newbuf;
        node->capacity = needed;
    }

    memcpy(node->data + offset, buf, size);

    if (needed > node->size) node->size = needed;

    return (int)size;
}

int tmpfs_truncate(vfs_node_t *node, u64 size)
{
    if (!node || node->type != VFS_FILE || size > VFS_MAX_FILE_SIZE) return -1;

    if (node->borrowed)
    {
        if (size > node->size) return -1;

        node->size = size;
        return 0;
    }

    if (size == 0)
    {
        if (node->data) kfree((u64 *)node->data);
        node->data = NULL;
        node->size = 0;
        node->capacity = 0;

        return 0;
    }

    if (size > node->capacity)
    {
        u8 *newbuf = (u8 *)kmalloc(size);

        if (!newbuf) return -1;
        if (node->data && node->size) memcpy(newbuf, node->data, node->size);
        if (node->data) kfree((u64 *)node->data);

        node->data = newbuf;
        node->capacity = size;
    }

    node->size = size;
    return 0;
}

void tmpfs_set_data(vfs_node_t *node, u8 *ptr, u64 size)
{
    if (!node || node->type != VFS_FILE) return;

    // free any previously owned buffer
    if (node->data && !node->borrowed) kfree((u64 *)node->data);

    node->data = ptr;
    node->size = size;

    node->capacity = size;
    node->borrowed = 1;
}

int tmpfs_read(vfs_node_t *node, void *buf, u64 size)
{
    if (!node || node->type != VFS_FILE || !buf) return -1;

    u64 to_copy = (size < node->size) ? size : node->size;
    if (to_copy > 0) memcpy(buf, node->data, to_copy);

    return (int)to_copy;
}
