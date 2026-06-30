/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 doccrLabs
 *
 * PROJECT: doccrOS
 * FILE: colors.h
 * CREATED BY: emex
 * MODIFIED BY: --
 *
 */

#ifndef COLORS_H
#define COLORS_H

//std
#define COLOR_BLACK   0xFF000000
#define COLOR_BG      0xFF1C1C1C
#define COLOR_RED     0xFFFF0000
#define COLOR_GREEN   0xFF00FF00
#define COLOR_YELLOW  0xFFFFFF00
#define COLOR_BLUE    0xFF0000FF
#define COLOR_PURPLE  0xFF800080
#define COLOR_CYAN    0xFF00FFFF
#define COLOR_WHITE   0xFFFFFFFF

//greys
#define COLOR_GRAY_10     0xFF1A1A1A
#define COLOR_GRAY_20     0xFF2E2E2E
#define COLOR_GRAY_30     0xFF4B4B4B
#define COLOR_GRAY_40     0xFF606060
#define COLOR_GRAY_50     0xFF757575
#define COLOR_GRAY_60     0xFF8A8A8A
#define COLOR_GRAY_70     0xFFA0A0A0
#define COLOR_GRAY_80     0xFFB5B5B5
#define COLOR_GRAY_90     0xFFCACACA

#include <types.h>

typedef u32 color_t;

//std
static inline color_t black(void)  { return 0xFF000000; }
static inline color_t red(void)    { return 0xFFFF0000; }
static inline color_t green(void)  { return 0xFF00FF00; }
static inline color_t yellow(void) { return 0xFFFFFF00; }
static inline color_t blue(void)   { return 0xFF0000FF; }
static inline color_t purple(void) { return 0xFF800080; }
static inline color_t cyan(void)   { return 0xFF00FFFF; }
static inline color_t white(void)  { return 0xFFFFFFFF; }

//greys
static inline color_t bg(void)     { return 0xFF1C1C1C; }
static inline color_t gray10(void) { return 0xFF1A1A1A; }
static inline color_t gray20(void) { return 0xFF2E2E2E; }
static inline color_t gray30(void) { return 0xFF4B4B4B; }
static inline color_t gray40(void) { return 0xFF606060; }
static inline color_t gray50(void) { return 0xFF757575; }
static inline color_t gray60(void) { return 0xFF8A8A8A; }
static inline color_t gray70(void) { return 0xFFA0A0A0; }
static inline color_t gray80(void) { return 0xFFB5B5B5; }
static inline color_t gray90(void) { return 0xFFCACACA; }

#define TEXT_COLOR GFX_GRAY_50
#define TITLE_COLOR GFX_WHITE
#define FAIL_COLOR GFX_RED
#define PASS_COLOR GFX_GREEN

#endif
