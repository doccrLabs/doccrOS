/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: sfence.h
 *
 */

#pragma once

static inline void store_fence()
{
    __asm__ volatile("sfence" ::: "memory");
}