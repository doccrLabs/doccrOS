/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: vfs.h
 *
 */

#ifndef VFS_H
#define VFS_H

#include <types.h>

#define VFS_NAME_MAX      64
#define VFS_MAX_CHILDREN  64   // TODO: do this dynamic
#define VFS_MAX_PATH      256

#define ROOT "/"

struct device_handler;

typedef enum {
    VFS_FILE,
    VFS_DIRECTORY,
    VFS_DEVICE
} vfs_type_t;

typedef struct vfs_node {
    char name[VFS_NAME_MAX];
    vfs_type_t     type;

    u8      *data;      // only files get to have data, directories just hold hands xds
    u64     size;       // bytes used
    u64     capacity;
    u8      borrowed;

    struct device_handler *device; // only set when type == VFS_DEVICE

    struct vfs_node     *parent;
    struct vfs_node     *children[VFS_MAX_CHILDREN];
    int child_count;
} vfs_node_t;

// core
void vfs_init(void);
vfs_node_t *vfs_get_root(void);

vfs_node_t *vfs_create_node(vfs_node_t *parent, const char *name, vfs_type_t type);
vfs_node_t *vfs_mkdir(const char *path);

int vfs_remove(const char *path);
int vfs_rename(const char *oldpath, const char *newpath);

vfs_node_t *vfs_find(const char *path);
vfs_node_t *vfs_find_child(vfs_node_t *dir, const char *name);
// a/b/c to a/b/ dirpath, but device/proc is "c"
void vfs_split_path(const char *path, char *dirpath_out, char *fname_out);

void vfs_list(vfs_node_t *dir);
void vfs_dump(void);

#define RDROOT "initrd.cpio"
#define RDHOME "initrdh.cpio"

int is_module_available();
int limine_module_find(char *module_path, const char *target_path);
void rootfs_init(void);

#endif
