/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 doccrLabs
 *
 * PROJECT: doccrOS
 * FILE: kernel.c
 * CREATED BY: emex
 * MODIFIED BY: --
 *
 */

#include "screen/bootscreen/boot.h"
#include <kernel/arch/hal/assembly.h>
#include <kernel/limine/reqs.h>
#include <kernel/include/logo.h>
#include <kernel/communication/serial.h>

#include <kernel/screen/graphics.h>
#include <kernel/screen/lib/print.h>
#include <kernel/screen/lib/string.h>
#include <kernel/screen/lib/log.h>
#include <kernel/screen/bootscreen/boot.h>

// CPU
#include <kernel/arch/x86_64/cpu.h>
#include <kernel/arch/x86_64/gdt/gdt.h>
#include <kernel/arch/x86_64/idt/idt.h>
#include <kernel/arch/x86_64/exceptions/irq.h>
#include <kernel/arch/x86_64/exceptions/timer.h>
#include <kernel/arch/x86_64/exceptions/panic.h>
#include <kernel/pci/pci.h>

// Memory
#include <kernel/mem/meminclude.h>

// Processes
#include <kernel/proc/process.h>
#include <kernel/proc/scheduler.h>

// modules
#include <kernel/devices/device_init.h>

void _start(void)
{
    // doccrOS start
    // Ensure that Limine base revision is supported and that we have a framebuffer
    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // Initialize framebuffer graphics
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    graphics_init(fb);
    init_bootscreen();
    draw_logo();

    // main kernel
    printf("==============================================\n");
    printf("==                                          ==\n");
    printf("==                  doccrOS                 ==\n");
    printf("==                                          ==\n");
    printf("==============================================\n");

    char buf[256]; //for all string operations

    {
        // Initialize mem
        physmem_init(memmap_request.response, hhdm_request.response);
        paging_init(hhdm_request.response);
        kheap_init();
    }

    draw_logo();

    {
        // Initialize the CPU
        cpu_detect();
        print(" [CPU] Detected CPU\n", COLOR_GREEN);
        gdt_init();
        print(" [GDT] init (Global Descriptor Table)\n", COLOR_GREEN);
        idt_init();
        print(" [IDT] Init interrupts\n", COLOR_GREEN);
        timer_init(1000);
        timer_set_boot_time(); //for uptime command
        //TODO:
        // its not exactly uptime because everything before imer_set_boot_time() doesnt get count
        // so we could set it to +50 milliseconds or something so its a bit more realistic i think...
        print(" [TIME] Init timer (1000Hz 1ms tick)\n", COLOR_GREEN);
        print(" [TIME] started uptime now...\n", COLOR_GREEN);
    }

    putchar('\n', COLOR_WHITE);

    pci_init();
    //char buf[64]; //its now at the top for every string operations
    str_copy(buf, " [PCI] ");
    str_append_uint(buf, pci_get_device_count());
    str_append(buf, " device(s) found\n");
    print(buf, COLOR_GREEN);
    //pci will get really useful with xhci/other usb
    //

    // Initialize process manager and scheduler
    process_init();
    print(" [PROC]  Process manager\n", COLOR_GREEN);
    sched_init();
    print(" [SCHED] Scheduler\n", COLOR_GREEN);

    putchar('\n', COLOR_WHITE);
    module_init();
    print(" [MOD] Module system initialized\n", COLOR_GREEN);

    hcf();

    //should not reach here
    #ifdef USE_HCF
        hcf();
    #else
        panic("USE_HCF; FAILED --> USING PANIC");
    #endif
}
