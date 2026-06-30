/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 doccrLabs
 *
 * PROJECT: doccrOS
 * FILE: boot.h
 * CREATED BY: emex
 * MODIFIED BY: --
 *
 */

//ajhsndjasbhdailshbdn jnashldbnkaskhdn


#ifndef BOOT_H
#define BOOT_H

#include <kernel/screen/colors.h>
#define BOOTSCREEN_BACKGROUND    COLOR_BLACK

// bootscreen vt's
#define BS_MAX_SCREENS 4
#define BS_LOG_MAX 65536
#define BS1 	0
#define BS2 	1
#define BS3 	2
#define BS4 	3

typedef struct {
	u32 cursor_x;
	u32 cursor_y;
	u32 x;
	u32 y;
	u32 width;
	u32 height;
	int visible;
	u32 *buffer; // opt. buffering
	//u32 *bg_buffer;
} bs_screen_t;

extern bs_screen_t bs_screens[BS_MAX_SCREENS];
extern int bs_active;

void init_bootscreen(void);
void uninit_bootscreen(void);

void bs_init_screens(void);
void bs_switch(int id);
void bs_set_region(int id, u32 x, u32 y, u32 w, u32 h);
bs_screen_t* bs_get_active(void);

/* backbuffer API used by print.c */
u32 *bs_backbuf_get(void);
u32 bs_backbuf_pitch_dw(void);
u32 bs_backbuf_height(void);

void bs_flush_rows(u32 y, u32 row_count);
void bs_flush_rect(u32 x, u32 y, u32 w, u32 h);
void bs_flush_rect_screen(int id, u32 x, u32 y, u32 w, u32 h);
void bs_backbuf_flush_all(void);
void bs_backbuf_clear(u32 color);
void bs_setpixel(bs_screen_t *scr, u32 lx, u32 ly, u32 color);
void bs_clear_screen(int id, u32 color);


#endif
