/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: s4
 * FILE: libdesktop.h
 */

#pragma once

#include <sys/shm.h>

//dt input:
#define DT_EV_NONE    0
#define DT_EV_MOUSE   1
#define DT_EV_KEY     2
#define DT_EV_RESIZE  3

#define DT_BTN_LEFT   0x01
#define DT_BTN_RIGHT  0x02
#define DT_BTN_MIDDLE 0x04

typedef struct
{
    unsigned char    type;
    unsigned char    buttons;
    unsigned char    modifiers;
    unsigned char    pressed;
    unsigned int    keycode;

    short mx;
    short my;
    signed char  scroll;

    int width;
    int height;
} dt_event_t;

#define DT_EVENT_QUEUE_MAX 64
#define DT_INPUT_PREFIX "/tmp/dt/input_"
//


#define DT_WIN     0x00
#define DT_POPUP   0x01
#define DT_NOMOVE  0x02
#define DT_NOTITLE 0x04
#define DT_NORESIZE 0x08

#define DT_TITLE_H 18
#define DT_BORDER   1

typedef struct
{
    int x;
    int y;
    int w;
    int h;
} DesktopArea;

static inline DesktopArea desktopContentArea(
    int x,
    int y,
    int w,
    int h,
    unsigned int style
){
    DesktopArea a;
    if (style & DT_POPUP) {
        a.x = x + 1;
        a.y = y + 1;
        a.w = w - 2;
        a.h = h - 2;
    } else if (style & DT_NOTITLE) {
        a.x = x + DT_BORDER;
        a.y = y + DT_BORDER;
        a.w = w - DT_BORDER * 2;
        a.h = h - DT_BORDER * 2;
    } else {
        // 2px border + 18px titlebar + 1px separator
        a.x = x + DT_BORDER;
        a.y = y + DT_TITLE_H + 1;
        a.w = w - DT_BORDER * 2;
        a.h = h - DT_TITLE_H - 1 - DT_BORDER;
    }
    return a;
}

static inline void desktopWindowSizeForContent(
    int content_w,
    int content_h,
    unsigned int style,
    int *out_w,
    int *out_h
) {
    if (style & DT_POPUP)
    {
        *out_w = content_w + 2;
        *out_h = content_h + 2;
    } else if (style & DT_NOTITLE)
    {
        *out_w = content_w + DT_BORDER * 2;
        *out_h = content_h + DT_BORDER * 2;
    } else
    {
        *out_w = content_w + DT_BORDER * 2;
        *out_h = content_h + DT_TITLE_H + 1 + DT_BORDER;
    }
}

#define DT_ABI_VERSION 3

typedef struct
{
	unsigned int abi_version;

    // register
    int  (*createWindow)(
        const char *title,
        int x,
        int y,
        int w,
        int h,
        unsigned int style
    );
    int (*createPopup)(
    	int x,
     	int y,
      	int w,
       	int h
    );

    // unregister
    void (*closeWindow)(void);
    void (*setTitle)(const char *title);
    void (*presentFrame)(void);
    void (*markDirty)(void);
    void (*winbuf_write)(const unsigned int *pixels, int w, int h);
    void (*getCurrentMousePos)(int *out_x, int *out_y);
    void (*getWindowSize)(int *out_w, int *out_h);
    int (*pollEvents)(dt_event_t *buf, int max);
    unsigned int *(*allocFramebuffer)(int w, int h);
    unsigned int *(*resizeFramebuffer)(int w, int h);
} Desktop;

extern Desktop desktop;

static inline int dt_check_abi(void)
{
	return desktop.abi_version == DT_ABI_VERSION;
}
