/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: rootfs.c
 *
 */

#include "vfs.h"
#include <kernel/limine/reqs.h>
#include <kernel/packages/cpio/cpio.h>
#include <kernel/screen/lib/string.h>
#include <kernel/screen/lib/print.h>

int is_module_available()
{
    if (
        module_request.response == NULL ||
        module_request.response->module_count == 0
    ) {
        log("[VFS]", "no boot modules found, rootfs stays empty and lonely\n");
        return -1;
    }
}

int limine_module_find(char *module_path, const char *target_path)
{
    if (is_module_available() < 0) return -1;

    for (u64 i = 0; i < module_request.response->module_count; i++)
    {
        struct limine_file *mod = module_request.response->modules[i];

        if (!mod || !mod->path) continue;

        if (str_contains(mod->path, module_path))
        {
            log("[VFS]", "found initrd module\n");
            cpio_extract(mod->address, mod->size, target_path);
            return 0;
        }
    }

    return -1;
}

void rootfs_init(void)
{
    log("[VFS]", "setting up rootfs\n");

    vfs_mkdir("/dev");
    vfs_mkdir("/tmp");
    vfs_mkdir("/bin");
    vfs_mkdir("/proc");
    vfs_mkdir("/users");
    vfs_mkdir("/system");

    limine_module_find(RDROOT, "/");
    limine_module_find(RDHOME, "/users");

    log("[VFS]", "initrd.cpio was not in the modules, sadly :/\n");
}
