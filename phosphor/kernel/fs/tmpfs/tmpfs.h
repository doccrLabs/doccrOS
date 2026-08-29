/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: tmpfs.h
 *
 */

#ifndef TMPFS_H
#define TMPFS_H

#include <types.h>
#include <kernel/fs/vfs/vfs.h>

#define VFS_MAX_FILE_SIZE (128 * 1024 * 1024)  // 128 MiB

vfs_node_t *tmpfs_create(const char *path);

int tmpfs_write(vfs_node_t *node, const void *buf, u64 size, u64 offset);
int tmpfs_read(vfs_node_t *node, void *buf, u64 size);
int tmpfs_truncate(vfs_node_t *node, u64 size);

// (not freed)
void tmpfs_set_data(vfs_node_t *node, u8 *ptr, u64 size);

#endif
