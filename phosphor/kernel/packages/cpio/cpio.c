/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: cpio.c
 *
 */

#include "cpio.h"
#include <kernel/fs/vfs/vfs.h>
#include <kernel/fs/tmpfs/tmpfs.h>
#include <kernel/screen/lib/string.h>
#include <kernel/screen/lib/print.h>
#include <kernel/communication/serial.h>

#define CPIO_LOG_BUF 320
typedef struct __attribute__((packed))
{
    char magic[6];
    char ino[8];
    char mode[8];
    char uid[8];
    char gid[8];
    char nlink[8];
    char mtime[8];
    char filesize[8];
    char devmajor[8];
    char devminor[8];
    char rdevmajor[8];
    char rdevminor[8];
    char namesize[8];
    char check[8];
} cpio_header_t;

#define CPIO_TRAILER          "TRAILER!!!"
#define CPIO_TYPE_MASK        0xF000
#define CPIO_TYPE_DIR         0x4000
#define CPIO_TYPE_REG         0x8000

static u32 hex_val(const char *s, int len)
{
    u32 val = 0;
    for (int i = 0; i < len; i++)
    {
        char c = s[i];
        val <<= 4;

        if (c >= '0' && c <= '9') val |= (u32)(c - '0' );
        else if (c >= 'a' && c <= 'f') val |= (u32)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val |= (u32)(c - 'A' + 10);
    }

    return val;
}

// "070701" or "070702" (with checksum), we dont care about checksums so both are fine
static int magic_ok(const char *m)
{
    return
        m[0] == '0' && m[1] == '7' && m[2] == '0' &&
        m[3] == '7' && m[4] == '0' &&
        (m[5] == '1' ||m[5] == '2')
    ;
}

static u64 align4(u64 x)
{
    return (x + 3) & ~3ULL;
}

void cpio_extract(void *archive, u64 size, const char *target_path)
{
    if (!archive || size < sizeof(cpio_header_t))
    {
        log("[CPIO]", "archive missing or way too small, skipping\n");
        return;
    }

    if (!target_path || target_path[0] == '\0')
        target_path = "/";

    {
        char msg[CPIO_LOG_BUF];
        char num[24];

        str_copy(msg, "archive at 0x");
        str_from_hex(num, (u64)archive);
        str_append(msg, num);
        str_append(msg, ", ");
        str_append_uint(msg, (u32)size);
        str_append(msg, " bytes total\n");

        log("[CPIO]", msg);
    }

    u8 *base = (u8 *)archive;
    u64 off = 0;
    u32 done = 0;

    while (off + sizeof(cpio_header_t) <= size)
    {
        cpio_header_t *hdr = (cpio_header_t *)(base + off);

        if (!magic_ok(hdr->magic))
        {
            log("[CPIO]", "bad magic, archive is toast\n");
            break;
        }

        u32 namesize = hex_val(hdr->namesize, 8);
        u32 filesize = hex_val(hdr->filesize, 8);
        u32 mode = hex_val(hdr->mode, 8);

        const char *name =
            (const char *)(base + off + sizeof(cpio_header_t));

        u64 data_off =
            align4(off + sizeof(cpio_header_t) + namesize);

        u64 next_off =
            align4(data_off + filesize);

        if (str_equals(name, CPIO_TRAILER))
        {
            printf("[CPIO] hit the trailer entry, thats a wrap\n");
            break;
        }

        if (next_off > size)
        {
            log("[CPIO]", "entry runs past end of archive, bailing\n");
            break;
        }

        if (name[0] == '.' && name[1] == '/')
            name += 2;

        if (name[0] != '\0' && !str_equals(name, "."))
        {
            char path[VFS_MAX_PATH];

            if (str_equals(target_path, "/"))
            {
                str_copy(path, "/");
                str_append(path, name);
            }
            else
            {
                str_copy(path, target_path);

                int len = str_len(path);

                if (len > 0 && path[len - 1] != '/')
                    str_append(path, "/");

                str_append(path, name);
            }

            u32 type = mode & CPIO_TYPE_MASK;

            if (type == CPIO_TYPE_DIR)
            {
                char msg[CPIO_LOG_BUF];

                str_copy(msg, "dir   ");
                str_append(msg, path);
                str_append(msg, "\n");

                log("[CPIO]", msg);

                vfs_mkdir(path);

                done++;
            }
            else if (type == CPIO_TYPE_REG)
            {
                char msg[CPIO_LOG_BUF];

                str_copy(msg, "file  ");
                str_append(msg, path);
                str_append(msg, " (");
                str_append_uint(msg, filesize);
                str_append(msg, " bytes)\n");

                log("[CPIO]", msg);

                vfs_node_t *f = tmpfs_create(path);

                if (f && filesize > 0) tmpfs_set_data(f, base + data_off, filesize);

                done++;
            }
            else
            {
                char msg[CPIO_LOG_BUF];
                char num[24];

                str_copy(msg, "skip  ");
                str_append(msg, path);
                str_append(msg, " (type 0x");
                str_from_hex(num, type);
                str_append(msg, num);
                str_append(msg, ", not a dir or regular file)\n");

                log("[CPIO]", msg, warning);
            }
        }

        off = next_off;
    }

    {
        char msg[CPIO_LOG_BUF];

        str_copy(msg, "unpacked ");
        str_append_uint(msg, done);
        str_append(msg, " entries total\n");

        log("[CPIO]", msg, success);
    }
}
