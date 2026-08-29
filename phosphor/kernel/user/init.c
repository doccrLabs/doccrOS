/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: init.c
 *
 */

#include "init.h"

#include <kernel/fs/vfs/vfs.h>
#include <kernel/packages/elf/elf.h>
#include <kernel/proc/process.h>
#include <kernel/screen/lib/print.h>
#include <kernel/screen/lib/log.h>
#include <kernel/screen/graphics.h>
#include <kernel/communication/serial.h>

#define SULFD_PATH "/system/bin/sulfd.elf"
#define SULFD_NAME "sulfd.elf"

static void load_elf(const char *path, const char *name, u64 initial_caps)
{
    vfs_node_t *node = vfs_find(path);

    if (!node)
    {
        printf("[USER] could not find %s, skipping...\n", path);
        return;
    }

    if (node->type != VFS_FILE)
    {
        log("[USER]", "path is not a file, skipping...\n", warning);
        return;
    }

    if (!node->data || node->size == 0)
    {
        log("[USER]", "binary is empty, skipping...\n", warning);
        return;
    }

    printf("[USER] found, load '%s' <%llu bytes>\n", path, node->size);

    int rc = elf_load(node->data, node->size, name, initial_caps, NULL);
    if (rc != 0)
    {
        log("[USER]", "could not load binary...\n", warning);
        return;
    }

    log("[USER]", "loading was a success!\n\n", success);
}

void user_start(void)
{
    bs.Clear(BS1);
    bs.Clear(BS2);
    bs.Clear(BS3);
    bs.Clear(BS4);
    clear(0xFF000000);
    load_elf(SULFD_PATH, SULFD_NAME, CAP_FRAMEBUFFER);
}
