#ifndef LOGO_H
#define LOGO_H

#include <types.h>

extern const u32 logo_width;
extern const u32 logo_height;
extern const u8 logo[];

void draw_logo(void);

#define LOGO_SCALE 6

extern const char *small_logo_text;
//extern const char *big_logo;

#endif
#pragma once
