/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: layout.c
 *
 */

#include "boot.h"
#include <kernel/screen/graphics.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/phys/physmem.h>
#include <kernel/mem/paging/paging.h>

static char bs_buf_bs1[BS_BUF_SIZE];
static char bs_buf_bs2[BS_BUF_SIZE];
static char bs_buf_bs3[BS_BUF_SIZE];
static char bs_buf_bs4[BS_BUF_SIZE];
//static u32 bs_pix_bs1[BS_MAX_PIXELS];
//static u32 bs_pix_bs2[BS_MAX_PIXELS];
//static u32 bs_pix_bs3[BS_MAX_PIXELS];
//static u32 bs_pix_bs4[BS_MAX_PIXELS];

static void bs_size_clamp(u32 *width, u32 *height)
{
    if (*width == 0) { *height = 0; return; }
    if ((u64)(*width) * (*height) <= BS_MAX_PIXELS) return;

    *height = BS_MAX_PIXELS / (*width);
}

static void bs_setup_screen
(
    int     screen,
    u32     *pixels,
    char    *buffer,

    u32  x,
    u32  y,
    u32  width,
    u32  height
) {
    bs_size_clamp(&width, &height);

    bs.Screens[screen].visible     = 1;
    bs.Screens[screen].cursor_x    = 0;
    bs.Screens[screen].cursor_y    = 0;
    bs.Screens[screen].buffer      = buffer;
    bs.Screens[screen].x           = x;
    bs.Screens[screen].y           = y;
    bs.Screens[screen].width       = width;
    bs.Screens[screen].height      = height;
    bs.Screens[screen].pixels      = pixels;
    bs.Screens[screen].stride      = width;
    bs.Screens[screen].direct      = 0;
    bs.Screens[screen].pixels_phys = 0;
    bs.Screens[screen].pixel_count = width * height;
}

static void bs_setup_screen_direct
(
    int     screen,
    char    *buffer,

    u32  x,
    u32  y,
    u32  width,
    u32  height
) {
    u32 fb_w = get_fb_width();
    u32 fb_h = get_fb_height();
    u32 fb_stride = get_fb_pitch() / 4;
    u32 *fb = get_framebuffer();

    if (x >= fb_w || y >= fb_h)
    {
        width  = 0;
        height = 0;
    }
    else
    {
        if (x + width  > fb_w) width  = fb_w - x;
        if (y + height > fb_h) height = fb_h - y;
    }

    bs.Screens[screen].cursor_x    = 0;
    bs.Screens[screen].cursor_y    = 0;
    bs.Screens[screen].buffer      = buffer;
    bs.Screens[screen].x           = x;
    bs.Screens[screen].y           = y;
    bs.Screens[screen].width       = width;
    bs.Screens[screen].height      = height;
    bs.Screens[screen].stride      = fb_stride;
    bs.Screens[screen].direct      = 1;
    bs.Screens[screen].pixels_phys = 0;
    bs.Screens[screen].pixel_count = width * height;
    bs.Screens[screen].pixels = (fb && width && height)
        ? (fb + (u64)y * fb_stride + x)
        : NULL
    ;
}

void bootscreen_layout_init(void)
{
    u32 fw      = get_fb_width();
    u32 fh      = get_fb_height();
    u32 half    = fw / 2;
    u32 mid     = fh / 2;

    bs.ScreensVisible(BS1, 1);
    bs.ScreensVisible(BS2, 1);
    bs.ScreensVisible(BS3, 1);
    bs.ScreensVisible(BS4, 1);

    // BS1 left
    bs_setup_screen_direct(
    	BS1, //screen
      	bs_buf_bs1, // buffer
       	0,    //x
        0,    //y
        half, // w
        fh    // h
    );

    // BS2 right
    bs_setup_screen_direct(
    	BS2,
      	bs_buf_bs2,
       	half,
        0,
        half,
        mid
    );

    // BS3 whole screen for fb0//tty
    bs_setup_screen(
    	BS3,
     	NULL,
      	bs_buf_bs3,
       	0,
        0,
        fw,
        fh
    );

    // BS4 also for userspace ig but idk
    bs_setup_screen_direct(
    	BS4,
      	bs_buf_bs4,
       	half,
        mid,
        half,
        fh - mid
    );

    bs.SwitchScreen(BS1);
}

void bootscreen_bs3_init_backbuffer(void)
{
    bs_screen_t *scr  = &bs.Screens[BS3];

    u64 size = (u64)scr->width * scr->height * sizeof(u32);
    u64 page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    u64 phys  = physmem_alloc_to(page_count);
    if (!phys)
    {
        log(
        	"[BOOT]",
         	"could not allocate BS3 backbuffer, fb0 will stay unavailable\n",
          	warning
        );
        return;
    }

    u64 hhdm = paging_get_hhdm_offset();

    scr->pixels = (u32 *)(phys + hhdm);
    scr->pixels_phys = phys;
    scr->stride = scr->width;
    scr->direct = 0;

    memset(
    	scr->pixels,
     	0,
      	page_count * PAGE_SIZE
    );

    log("[BOOT]", "BS3 backbuffer ready\n", success);
}