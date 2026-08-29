/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: procfs.c
 *
 */

#include "procfs.h"
#include <kernel/fs/vfs/vfs.h>
#include <kernel/devices/device_init.h>
#include <kernel/screen/lib/string.h>

typedef struct
{
    char name[VFS_NAME_MAX];
    procfs_gen_t generator;

} procfs_entry_t;

static procfs_entry_t procfs_entries[PROCFS_MAX_ENTRIES];
static int procfs_entry_count = 0;

static void *procfs_open(const char *path)
{
    if (!path) return NULL;

    const char *base = path;
    int i = 0;

    for (const char *s = path; *s; s++)
    {
        if (*s == '/') base = s + 1;
    }

    for (i = 0; i < procfs_entry_count; i++)
    {
        if (str_equals(procfs_entries[i].name, base))
        {
            return &procfs_entries[i];
        }
    }

    return NULL;
}

static void procfs_close(void *handle)
{
    (void)handle;
}

static int procfs_read(void *handle, void *buf, size_t count)
{
    procfs_entry_t *entry = (procfs_entry_t *)handle;

    if (!entry || !entry->generator || !buf) return -1;

    return entry->generator((char *)buf, (u64)count);
}

static device_handler procfs_device = {
    .name    = "procfs",
    .mount   = NULL,
    .version = VERSION_NUM(1, 0, 0, 0),
    .init    = NULL,
    .fini    = NULL,
    .open    = procfs_open,
    .close   = procfs_close,
    .read    = procfs_read,
    .write   = NULL,
    .ioctl   = NULL,
};

int procfs_register(const char *name, procfs_gen_t generator)
{
    if (!name || !generator) return -1;
    if (procfs_entry_count >= PROCFS_MAX_ENTRIES)
    {
        log("[PROC]", "too many entries..\n", warning);
        return -1;
    }

    // /proc should already exist by the time entries register themselves,
    // but just in case someone calls this before procfs_init did this idk
    vfs_node_t *dir = vfs_find("/proc");
    if (!dir) dir = vfs_mkdir("/proc");
    if (!dir)
    {
        log("[PROC]", "no /proc dir, couldnt register\n", warning);
        return -1;
    }

    vfs_node_t *node = vfs_create_node(dir, name, VFS_DEVICE);
    if (!node)
    {
        log("[PROC]", "couldnt create vfsnode, because of\n", warning); // TODO: cuz of??
        return -1;
    }

    node->device = &procfs_device;

    procfs_entry_t *entry = &procfs_entries[procfs_entry_count];
    str_copy(entry->name, name);
    entry->generator = generator;

    procfs_entry_count++;
    return 0;
}

void procfs_init(void)
{
    log("[PROC]", "init procfs\n");

    vfs_mkdir("/proc");
    procfs_dir_init();
}
