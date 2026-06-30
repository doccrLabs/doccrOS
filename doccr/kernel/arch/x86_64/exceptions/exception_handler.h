/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 doccrLabs
 *
 * PROJECT: doccrOS
 * FILE: exception_handler.h
 * CREATED BY: emex
 * MODIFIED BY: --
 *
 */

#ifndef EXCEPTION_HANDLER_H
#define EXCEPTION_HANDLER_H

#include <types.h>
#include"../idt/idt.h"

extern const char* exception_messages[32];


void exception_handler(cpu_state_t *state);

#endif
