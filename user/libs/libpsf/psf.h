/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: s4
 * FILE: psf.h
 */

#pragma once

#include "psf.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    uint32_t codepoint;
    uint32_t glyph;
} psf_unicode_entry_t;

typedef struct
{
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t glyph_count;
    uint32_t bytes_per_glyph;
    uint32_t bytes_per_row;

    uint8_t *glyphs;

    psf_unicode_entry_t *unicode_entries;
    uint32_t unicode_entry_count;
} psf_font_t;


int psf_load(const char *path, psf_font_t *font);
int psf_glyph_index(const psf_font_t *font, uint32_t codepoint);
int psf_get_pixel(const psf_font_t *font, uint32_t glyph_index, int x, int y);
int psf_width(const psf_font_t *font);
int psf_height(const psf_font_t *font);

void psf_free(psf_font_t *font);

uint32_t psf_glyph_row_bits(const psf_font_t *font, uint32_t glyph_index, int row);
