/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: sulfd.c
 *
 */

#include "sulfd.h"
#include "rc.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <libcpio.h>

int main(void)
{
    struct utsname uts;
    cpio_extract_stats_t stats;

    if (uname(&uts) != 0)
    {
        fprintf(stderr, "failed to get system information\n");
        return 1;
    }

    printf("System information:\n");
    printf("sulfurOS release: %s\n", uts.release);
    printf("sulfurOS version/build: %s\n", uts.version);
    printf("\n");

    #define USERFIRST "/users/user_id"
    #define NAMEAFTER "/users/pc"
    printf("setting up users directory\n");
    printf("renaming %s to %s... \n", USERFIRST, NAMEAFTER);
    if (rename(USERFIRST, NAMEAFTER) < 0) return 1;

    #define SRC_CPIO "/system/s4.cpio"
    #define DEST_DIR "/system/desktop/resources/"

    printf("unzipping %s to %s...\n", SRC_CPIO, DEST_DIR);

    if (cpio_extract_file(SRC_CPIO, DEST_DIR, &stats) != 0)
    {
        printf("couldnt extract archive\n", SRC_CPIO);
        return 1;
    }

    printf(
        ":: finished: %d entries, (%d files; %d dirs)\n",
        stats.total_entries,
        stats.files_written,
        stats.dirs_created
    );


    static sulfd_t rc;

    if (sulfd_parse(SULFD_PATH, &rc) != 0)
    {
        fprintf(
            stderr,
            __SULFD_BRACKETS " .sulfd not found, using defaults!\n"
        );

        if (spawn(DESKTOP) < 0) goto error;

        return 0;
    }

    sulfd_run(&rc);
    return 0;

error:
    fprintf(
        stderr,
        __SULFD_BRACKETS " all execs failed, halting\n"
    );

    return 0;
}