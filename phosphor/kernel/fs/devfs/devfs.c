/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: devfs.c
 *
 */

#include "devfs.h"
#include <kernel/fs/vfs/vfs.h>

vfs_node_t *devfs_mount(const char *path, struct device_handler *device)
{
    char dirpath[VFS_MAX_PATH];
    char fname[VFS_NAME_MAX];
    if (!path || !device) return NULL;

    vfs_split_path(path, dirpath, fname);

    vfs_node_t *dir = vfs_find(dirpath);

    if (fname[0] == '\0') return NULL;
    if (!dir) dir = vfs_mkdir(dirpath);
    if (!dir) return NULL;

    vfs_node_t *node = vfs_create_node(dir, fname, VFS_DEVICE);
    if (!node) return NULL;

    node->device = device;
    return node;
}
