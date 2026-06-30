/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 doccrLabs
 *
 * PROJECT: doccrOS
 * FILE: reqs.h
 * CREATED BY: emex
 * MODIFIED BY: --
 *
 */

#ifndef REQS_H
#define REQS_H

#include <limine/limine.h>

// Limine requests
extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;

#endif
