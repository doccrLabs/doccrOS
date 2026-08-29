/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: vt.h
 *
 */

#pragma once

#include <types.h>
#include <kernel/devices/device_init.h>

#define VT_MAX 64
#define VT_PATH_MAX 32
#define VT_QUEUE_SIZE 256

// ioctl calls to /dev/vt/ctl
#define VT_IOCTL_CREATE  0   // out: new vt id
#define VT_IOCTL_DESTROY 1   // in: delete id, set id free for other processes
#define VT_IOCTL_GET_ACTIVE 2 // out: active id (0 = no id)
#define VT_IOCTL_SET_ACTIVE 3 // in: activate an id

// ioctl calls to /dev/vt/<id>
#define VT_IOCTL_GET_ID   4 // out: get id
#define VT_IOCTL_ACTIVATE   5 // set visible on screen
#define VT_IOCTL_DEACTIVATE 6 // set unvisible for screen
#define VT_IOCTL_FEED     7  // feed to read() vt_feed_args_t
#define VT_IOCTL_CLEAR    8
#define VT_IOCTL_READ_OUTPUT 9 //what the vt*id has so far

typedef struct
{
    const void *data;
    u64 len;
} vt_feed_args_t;
typedef struct
{
    void *data;
    u64 len;
} vt_drain_args_t;

void vt_manager_init(void);
void vt_screen_set_enabled(int enabled);
int vt_screen_enabled(void);

void vt_kbd_feed(u16 code, u8 modifiers);
extern device_handler vt_ctl_device;
