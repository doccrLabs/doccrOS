/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: s4
 * FILE: bmp.h
 *
 */

#pragma once

/*
 * 24bpp and 32bpp , uncompressed BMP
 *
 */

//#include "../../compositor/comp.h"

typedef struct bmp_image_s
{
    int width;
    int height;
    unsigned int *pixels; /* ARGB 0xAARRGGBB; top-down */
} bmp_image_t;

typedef struct
{
    void (*set)(void *ctx, int x, int y, unsigned int color); // set where it should draw to
    unsigned int (*get)(void *ctx, int x, int y);
    void *ctx;
} bmp_target_t;


int bmp_load(
    const char *path,
    bmp_image_t *img
);

void bmp_free(
    bmp_image_t *img
);

void bmp_draw(
    const bmp_target_t *target,
    const bmp_image_t *img,
    int x,
    int y
);

void bmp_draw_scaled(
    const bmp_target_t *target,
	const bmp_image_t *img,
    int x,
    int y,
    int w,
    int h
);

void bmp_draw_ex(
    const bmp_target_t *target,
	const bmp_image_t *img,
    int x,
    int y,
    int w,
    int h,
    int sat,
    int bright,
    int alpha
);