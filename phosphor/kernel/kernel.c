/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: kernel.c
 *
 */

#include "kernel.h"
#include "arch/x86_64/assembly.h"
#include "devices/init.h"
#include "screen/lib/print.h"

#include <kernel/arch/hal/assembly.h>
#include <kernel/limine/reqs.h>
#include <kernel/communication/serial.h>

#include <kernel/screen/graphics.h>
#include <kernel/screen/lib/print.h>
#include <kernel/screen/lib/string.h>
#include <kernel/screen/lib/log.h>
#include <kernel/screen/bootscreen/boot.h>

// CPU
#include <kernel/arch/hal/cpu.h>
#include <kernel/arch/hal/interrupts.h>
#include <kernel/arch/hal/timer.h>
#include <kernel/arch/hal/panic.h>
#include <kernel/pci/pci.h>

// Memory
#include <kernel/mem/meminclude.h>
#include <kernel/mem/vmm/vmm.h>

// Processes
#include <kernel/proc/process.h>
#include <kernel/proc/thread.h>
#include <kernel/proc/demo_threads.h>
#include <kernel/proc/scheduler.h>

// modules
#include <kernel/devices/device_init.h>
#include <kernel/devices/init.h>

// File system
#include <kernel/fs/vfs/vfs.h>
#include <kernel/fs/procfs/procfs.h>
#include <kernel/fs/devfs/devfs.h>
#include <kernel/fs/tmpfs/tmpfs.h>
#include <kernel/fs/openfile.h>

// User
#include <kernel/user/init.h>

void _start(void)
{
    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // Initialize framebuffer graphics
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    serial_init();
    graphics_init(fb);

    //full buffer clear even tho they werent used yet
    bs.Clear(BS1);
    bs.Clear(BS2);
    bs.Clear(BS3);
    bs.Clear(BS4);

    print("Welcome to ", white());
    print("phosphor", blue());
    print("!\n\n", white());

    {
        physmem_init(memmap_request.response, hhdm_request.response);
        paging_init(hhdm_request.response);
        kheap_init();
        vmm_init();
    }

    bootscreen_bs3_init_backbuffer();

    {
        cpu_detect();
        log("[CPU]", "Detected CPU\n");
        cpu_early_init();
        vmm_cow_install_handler();
        timer_init(1000);
        timer_set_boot_time(); //for uptime command
    }

    pci_init();
    process_init();
    sched_init();

    {
        vfs_init();
        rootfs_init();
        procfs_init();
        devices_init();
        init_all_devices();
        vfs_dump();
    }


    proc_t *kproc = process_create("kernel");
    thread_t *t_rt = thread_create(kproc, "__rt", idle_fn, NULL);
    if (!kproc) panic("could not create kernel proc, rip");
    if (!t_rt) panic("could not create \"__rt\" thread");

    sched_set_idle(t_rt);

    user_start();
    sched_start();

    //should not reach here
    #if USE_HCF == 1
        hcf();
    #else
        panic("USE_HCF; FAILED --> USING PANIC");
    #endif
}
