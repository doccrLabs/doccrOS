/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: dir.c
 *
 */

#include "procfs.h"
#include <kernel/arch/hal/cpu.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/phys/physmem.h>
#include <kernel/screen/lib/string.h>

#define PROCFS_LINE_BUF  512

static int procfs_cpuinfo_read(char *buf, u64 size)
{
    if (!buf || size == 0) return 0;

    char tmp[PROCFS_LINE_BUF];
    tmp[0] = '\0';

    str_append(tmp, "vendor_id\t: ");
    str_append(tmp, cpu_get_vendor());
    str_append(tmp, "\n");

    str_append(tmp, "model name\t: ");
    str_append(tmp, cpu_get_brand());
    str_append(tmp, "\n");

    str_append(tmp, "arch\t\t: x86_64\n"); // we currently only have x86_64 support
    //TODO:
    // not todo, more like reminder to add more architectures
    str_append(tmp, "processors\t: 1\n"); // cuz theres no smp yet

    u64 len = (u64)str_len(tmp);
    if (len > size) len = size;

    memcpy(buf, tmp, len);
    return (int)len;
}

static int procfs_meminfo_read(char *buf, u64 size)
{
    if (!buf || size == 0) return 0;

    char tmp[PROCFS_LINE_BUF];
    tmp[0] = '\0';

    u64 heap_total = mem_get_total();
    u64 heap_used = mem_get_used();
    u64 heap_free = mem_get_free();
    u64 phys_free_kb = (physmem_free_get() * PAGE_SIZE) / 1024;


    str_append(tmp, "MemTotal:\t");
    str_append_uint(tmp, (u32)(heap_total / 1024));
    str_append(tmp, " kB\n");

    str_append(tmp, "MemFree:\t");
    str_append_uint(tmp, (u32)(heap_free / 1024));
    str_append(tmp, " kB\n");

    str_append(tmp, "MemUsed:\t");
    str_append_uint(tmp, (u32)(heap_used / 1024));
    str_append(tmp, " kB\n");

    str_append(tmp, "PhysFree:\t");
    str_append_uint(tmp, (u32)phys_free_kb);
    str_append(tmp, " kB\n");


    u64 len = (u64)str_len(tmp);
    if (len > size) len = size;

    memcpy(buf, tmp, len);
    return (int)len;
}

void procfs_dir_init(void)
{
    procfs_register("cpuinfo", procfs_cpuinfo_read);
    procfs_register("meminfo", procfs_meminfo_read);
}
