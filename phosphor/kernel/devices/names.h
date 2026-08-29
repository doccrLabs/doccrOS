/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: names.h
 *
 */

#pragma once

#include "device_init.h"

#define FB0_NAME "framebuffer0" // theres only one framebuffer normaly lol
#define FB0_MOUNT "/dev/fb0"
#define FB0_VERSION VERSION_NUM(1, 0, 0, 0)

#define KBD_NAME "keyboard_dev0" // we have different keyboards which means its the first one (0)
#define KBD_MOUNT "/dev/kbd0" // its a event the userspace gets
#define KBD_VERSION VERSION_NUM(1, 0, 0, 0)

#define MOUSE_NAME "mouse_dev0"
#define MOUSE_MOUNT "/dev/mouse"
#define MOUSE_VERSION VERSION_NUM(1, 0, 0, 0)

#define SHM_NAME "shm_dev0"
#define SHM_MOUNT "/dev/shm0"
#define SHM_VERSION VERSION_NUM(1, 0, 0, 0)

//TODO:
// fkin multi vt's
#define VT_ID_MOUNT "/dev/vt/"
#define VT_CTL_NAME "vt_ctl0"
#define VT_CTL_MOUNT "/dev/vt/ctl" // control system for virtual terminals
#define VT_CTL_VERSION VERSION_NUM(1, 0, 0, 0)