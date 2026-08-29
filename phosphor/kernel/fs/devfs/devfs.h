/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: devfs.h
 *
 */

#ifndef DEVFS_H
#define DEVFS_H

#include <types.h>
#include <kernel/fs/vfs/vfs.h>

vfs_node_t *devfs_mount(const char *path, struct device_handler *device);

#endif
