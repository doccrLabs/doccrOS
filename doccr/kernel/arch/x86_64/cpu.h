/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 doccrLabs
 *
 * PROJECT: doccrOS
 * FILE: cpu.h
 * CREATED BY: emex
 * MODIFIED BY: --
 *
 */

#ifndef CPU_H
#define CPU_H

#include <types.h>

void cpu_detect(void);
const char* cpu_get_vendor(void);
const char* cpu_get_brand(void);

#endif
