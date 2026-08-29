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

#include <kernel/devices/device_init.h>

#include <drivers/x86_64/ps2/ps2.h>

#include <kernel/devices/input/ctrl.h>
#include <kernel/devices/input/kbd.h>
#include <kernel/devices/input/mouse.h>
#include <kernel/devices/fb/fb0.h>
#include <kernel/devices/shm/shm.h>
#include <kernel/devices/vt/vt.h>

#include <kernel/screen/lib/log.h>
#include <kernel/communication/serial.h>

void init_all_devices(void)
{
 	input_ctrl_init();
    device_register(&kbd_module);
    device_register(&mouse_module);
    device_register(&fb0_device);
    device_register(&vt_ctl_device);
    device_register(&shm_device);

    vt_manager_init();
    keyboard_init();
    ps2_mouse_init();
}
