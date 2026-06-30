/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 doccrLabs
 *
 * PROJECT: doccrOS
 * FILE: print.h
 * CREATED BY: emex
 * MODIFIED BY: --
 *
 */

#ifndef PRINT_H
#define PRINT_H

#include <types.h>
#include "../colors.h"
#include "log.h"
#include "string.h"

// text output functions
void putchar(char c, u32 color);
void string(const char *str, u32 color);
void print(const char *str, u32 color);
void printInt(int value, u32 color);

void reset_cursor(void);

#endif
