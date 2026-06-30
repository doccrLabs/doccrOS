/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 doccrLabs
 *
 * PROJECT: doccrOS
 * FILE: print.c
 * CREATED BY: emex
 * MODIFIED BY: --
 *
 */

#include "print.h"
#include <kernel/screen/graphics.h>
#include <kernel/screen/font_8x8.h>

static void putchar_at(char c, u32 x, u32 y, u32 color)
{
    const u8 *glyph = font_8x8[(u8)c];

    for (int dy = 0; dy < 8; dy++)
    {
        u8 row = glyph[dy];
        for (int dx = 0; dx < 8; dx++)
        {
            if (row & (1 << (7 - dx)))
            {
                // Draw scaled pixel
                for (u32 sy = 0; sy < font_scale; sy++) {
                    for (u32 sx = 0; sx < font_scale; sx++) {
                        putpixel(x + dx * font_scale + sx, y + dy * font_scale + sy, color);
                    }
                }
            }
        }
    }
}

void putchar(char c, u32 color)
{
    u32 char_width = 8 * font_scale;
    u32 char_height = 8 * font_scale;
    u32 char_spacing = char_width;
    u32 line_height = char_height + 2 * font_scale;

    if (c == '\n')
    {
        cursor_x = 20;
        cursor_y += line_height;

        return;
    }

    // Check if we need to wrap to next line
    if (cursor_x + char_width >= fb_width)
    {
        cursor_x = 20;
        cursor_y += line_height;
    }

    putchar_at(c, cursor_x, cursor_y, color);
    cursor_x += char_spacing;
}

void string(const char *str, u32 color)
{
    for (size_t i = 0; str[i]; i++)
    {
        putchar(str[i], color);
    }
}

void printInt(int value, u32 color)
{
    char buffer[12];
    IntToString(value, buffer);
    string(buffer, color);
}

void print(const char *str, u32 color)
{
    string(str, color);
    //putchar('\n', color);
}

void reset_cursor(void)
{
    cursor_x = 0;
    cursor_y = 0;
}
