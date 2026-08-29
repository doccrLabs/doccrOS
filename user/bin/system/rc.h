/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: rc.h
 *
 */

#pragma once

#define SULFD_MAX_VARS 255
#define SULFD_MAX_EXECS 255

#define SULFD_PATH "/system/.sulfd"

typedef struct {
    char name[64];
    char path[256];
    int value;
    int is_value;
} sulfd_var_t;

typedef struct {
    char var_name[64];
    char direct_path[256];
    int bg;
    int is_wait;
    int is_print;
    int is_elog;
    int is_if;
    int is_else;
    int is_endif;
    char message[256];
    int wait_time;
} sulfd_exec_t;

typedef struct {
    sulfd_var_t vars[SULFD_MAX_VARS];
    int var_count;
    sulfd_exec_t execs[SULFD_MAX_EXECS];
    int exec_count;
} sulfd_t;

int sulfd_parse(const char *path, sulfd_t *out);
void sulfd_run(sulfd_t *rc);
