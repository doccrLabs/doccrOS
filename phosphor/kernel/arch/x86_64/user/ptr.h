/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: ptr.h
 *
 */

#pragma once

int user_ptr_ok(u64 ptr);
int user_range_ok(u64 ptr, u64 len);
