/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: boot.c
 *
 */

#include "boot.h"
#include <kernel/screen/graphics.h>
#include <kernel/screen/font_8x8.h>
#include <kernel/mem/mem.h>

bootscreen_api_t bs; // the one and only instance

static int active_screen = BS1; // the screen for bs.Print()/bs.Putchar() rn
// in layout.c to change

static void bs_setpixel(bs_screen_t *scr, u32 x, u32 y, u32 color)
{
    if (!scr->pixels) return;
    if (x >= scr->width || y >= scr->height) return;

    scr->pixels[y * scr->stride + x] = color;
}


static void bs_draw_glyph(bs_screen_t *scr, char c, u32 color)
{
    const u8 *glyph = font_8x8[(u8)c];
    u32 scale = get_font_scale();

    for (int dy = 0; dy < 8; dy++)
    {
        u8 row = glyph[dy];

        for (int dx = 0; dx < 8; dx++)
        {
            if (!(row & (1 << (7 - dx)))) continue;

            for (u32 sy = 0; sy < scale; sy++)
            {
                for (u32 sx = 0; sx < scale; sx++)
                {
                    bs_setpixel
                    (
                        scr,
                        scr->cursor_x + dx * scale + sx,
                        scr->cursor_y + dy * scale + sy,
                        color
                    );
                }
            }
        }
    }
}

static void bs_flush_rect(bs_screen_t *scr, u32 x, u32 y, u32 w, u32 h)
{
    if (!scr->pixels) return;
    if (scr->direct) return;

    u32 end_x = x + w;
    u32 end_y = y + h;

    if (end_x > scr->width)  end_x = scr->width;
    if (end_y > scr->height) end_y = scr->height;
    if (x >= end_x || y >= end_y) return;

    u32 *fb = get_framebuffer();
    if (!fb) return;

    u32 fb_w      = get_fb_width();
    u32 fb_h      = get_fb_height();
    u32 fb_stride = get_fb_pitch() / 4;

    u32 dst_x0 = scr->x + x;
    u32 dst_y0 = scr->y + y;

    if (dst_x0 >= fb_w || dst_y0 >= fb_h) return;

    u32 copy_w = end_x - x;
    if (dst_x0 + copy_w > fb_w) copy_w = fb_w - dst_x0;

    u32 copy_rows = end_y - y;
    if (dst_y0 + copy_rows > fb_h) copy_rows = fb_h - dst_y0;

    for (u32 row = 0; row < copy_rows; row++)
    {
        u32 sy = y + row;
        u32 dy = dst_y0 + row;

        memcpy(
            &fb[dy * fb_stride + dst_x0],
            &scr->pixels[sy * scr->stride + x],
            copy_w * sizeof(u32)
        );
    }
}


static void bs_flush(bs_screen_t *scr)
{
    bs_flush_rect(scr, 0, 0, scr->width, scr->height);
}

static void bs_clear_area(bs_screen_t *scr)
{
    if (!scr->pixels) return;

    for (u32 row = 0; row < scr->height; row++)
    {
        u32 *r = scr->pixels + (u64)row * scr->stride;

        for (u32 col = 0; col < scr->width; col++) r[col] = black();
    }

    bs_flush(scr);
}

static void bs_scroll(bs_screen_t *scr)
{
    u32 scale  = get_font_scale();
    u32 line_h = 8 * scale + 2 * scale;

    if (!scr->pixels  || line_h >= scr->height)
    {
        // no backbuffer or the screen is too tiny to fit even one line, just wipe it
        bs_clear_area(scr);
        scr->cursor_x  = 0;
        scr->cursor_y  = 0;
        return;
    }

    u32 keep_rows = scr->height - line_h;

    if (scr->stride == scr->width)
    {
        memmove
        (
            scr->pixels,
            scr->pixels + (u64)line_h * scr->stride,
            (u64)keep_rows * scr->width * sizeof(u32)
        );

        u32 *bottom      = scr->pixels + (u64)keep_rows * scr->stride;
        u32 bottom_len   = line_h * scr->width;

        for (u32 i = 0; i < bottom_len; i++) bottom[i] = black();
    }
    else
    {
        for (u32 row = 0; row < keep_rows; row++)
        {
            memmove(
                scr->pixels + (u64)row * scr->stride,
                scr->pixels + (u64)(row + line_h) * scr->stride,
                scr->width * sizeof(u32)
            );
        }

        for (u32 row = keep_rows; row < scr->height; row++)
        {
            u32 *r = scr->pixels + (u64)row * scr->stride;

            for (u32 col = 0; col < scr->width; col++) r[col] = black();
        }
    }

    bs_flush(scr);

    scr->cursor_x = 0;
    scr->cursor_y = keep_rows;
}

typedef struct
{
    u32 y0;
    u32 y1;
    int active;
} bs_dirty_t;

static void bs_dirty_add(bs_dirty_t *dirty, u32 y0, u32 y1)
{
    if (!dirty) return;

    if (!dirty->active)
    {
        dirty->y0     = y0;
        dirty->y1     = y1;
        dirty->active = 1;
        return;
    }

    if (y0 < dirty->y0) dirty->y0 = y0;
    if (y1 > dirty->y1) dirty->y1 = y1;
}

static void bs_putchar_ex(char c, u32 color, int do_flush, bs_dirty_t *dirty)
{
    bs_screen_t *scr = &bs.Screens[active_screen];
    if (!scr->visible) return;

    #if BOOTUP_VISUALS == 0
        u32 scale = get_font_scale();
        u32 char_w = 8 * scale;
        u32 char_h = 8 * scale;
        u32 line_h = char_h + 2 * scale;
        int line_advanced = 0;

        if (c == '\n')
        {
            scr->cursor_x = 0;
            scr->cursor_y += line_h;
            line_advanced = 1;
        }
        else
        {
            if (scr->cursor_x + char_w >= scr->width)
            {
                scr->cursor_x = 0;
                scr->cursor_y += line_h;
                line_advanced = 1;
            }

            bs_draw_glyph(scr, c, color);

            if (do_flush)
            {
                bs_flush_rect(
                   	scr,
                   	scr->cursor_x,
                   	scr->cursor_y,
                   	char_w,
                    char_h
                );
            }
            else
            {
                bs_dirty_add(dirty, scr->cursor_y, scr->cursor_y + char_h);
            }

            scr->cursor_x += char_w;
        }

        if (
            line_advanced &&
            scr->cursor_y + line_h >= scr->height
        ) {
            bs_scroll(scr); // flushes the whole screen itself

            if (dirty) dirty->active = 0;
        }

        // keeps a copy in log from boot
        if (scr->buffer && scr->buf_len < BS_BUF_SIZE - 1)
        {
            scr->buffer[scr->buf_len++] = c;
            scr->buffer[scr->buf_len] = '\0';
        }
    #endif
}

static void bs_putchar(char c, u32 color)
{
    bs_putchar_ex(c, color, 1, 0);
}

static void bs_print(const char *str, u32 color)
{
    bs_screen_t *scr = &bs.Screens[active_screen];
    bs_dirty_t dirty = { 0, 0, 0 };

    for (
        size_t i = 0;
        str[i];
        i++
    ) {
        bs_putchar_ex(str[i], color, 0, &dirty);
    }

    if (scr->visible && dirty.active)
    {
        bs_flush_rect(scr, 0, dirty.y0, scr->width, dirty.y1 - dirty.y0);
    }
}

static void bs_putpixel(u32 x, u32 y, u32 color)
{
    bs_screen_t *scr   = &bs.Screens[active_screen];
    if (!scr->visible) return;

    bs_setpixel(scr, x, y, color);

    if (!scr->direct) putpixel(scr->x + x, scr->y + y, color);
}

static void bs_switch_screen(int screen)
{
    // so it doesnt crash when a wrong input is given
    if (
        screen < 0 || screen >= BS_COUNT
    ) return;

    active_screen = screen;
}

static void bs_screens_visible(int screen, int on)
{
    if (
        screen < 0 || screen >= BS_COUNT
    )return;

    bs.Screens[screen].visible = on;
}

static void bs_clear(int screen)
{
    if (screen < 0 || screen >= BS_COUNT) return;

    bs_screen_t *scr = &bs.Screens[screen];
    bs_clear_area(scr);

    scr->cursor_x     = 0;
    scr->cursor_y     = 0;
    scr->buf_len      = 0;
    if (scr->buffer) scr->buffer[0] = '\0';
}

static void bs_flush_screen(int screen)
{
    if (
    	screen < 0 || screen >= BS_COUNT
    ) return;

    bs_flush(&bs.Screens[screen]);
}

static void bs_init(void)
{
    memset(bs.Screens, 0, sizeof(bs.Screens));
    active_screen = BS1;
}

void bootscreen_setup(void)
{
    bs.Init               = bs_init;
    bs.SwitchScreen       = bs_switch_screen;
    bs.ScreensVisible     = bs_screens_visible;
    bs.Print              = bs_print;
    bs.Putchar            = bs_putchar;
    bs.Clear               = bs_clear;
    bs.Putpixel            = bs_putpixel;
    bs.Flush               = bs_flush_screen;

    bs.Init();
}
