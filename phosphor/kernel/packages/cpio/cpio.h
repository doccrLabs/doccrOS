/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: cpio.h
 *
 */

#ifndef CPIO_H
#define CPIO_H

#include <types.h>

void cpio_extract(void *archive, u64 size, const char *target_path);

#endif
