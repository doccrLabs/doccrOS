/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: libcpio.c
 *
 */

#include "libcpio.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define CPIO_TRAILER "TRAILER!!!"
#define CPIO_TYPE_MASK 0xF000
#define CPIO_TYPE_DIR 0x4000
#define CPIO_TYPE_REG 0x8000
#define CPIO_PATH_MAX 256 //vfs is limited
#define CPIO_OK 0
#define CPIO_ERR_ARGUMENT -1
#define CPIO_ERR_FORMAT -2
#define CPIO_ERR_IO -3
#define CPIO_ERR_PATH -4
#define CPIO_ERR_WRITE -5

typedef struct __attribute__((packed)) {
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

static unsigned int hex_val(const char *s, int len)
{
    unsigned int val = 0;

    for (int i = 0; i < len; i++)
    {
        char c = s[i];
        val <<= 4;

        if (c >= '0' && c <= '9') val |= (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f') val |= (unsigned int)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val |= (unsigned int)(c - 'A' + 10);
        else return 0;
    }

    return val;
}

static int magic_ok(const char *m)
{
    return
        m[0] == '0' &&
        m[1] == '7' &&
        m[2] == '0' &&
        m[3] == '7' &&
        m[4] == '0' &&
        (m[5] == '1' || m[5] == '2')
    ;
}

static unsigned long align4(unsigned long x)
{
    return (x + 3UL) & ~3UL;
}

static int join_path(
    char *out,
    int outsz,
    const char *dir,
    const char *name
) {
    int i = 0;
    int j = 0;
    int dlen = 0;

    if (!out || !dir || !name || outsz <= 0) return CPIO_ERR_ARGUMENT;
    while (dir[dlen]) dlen++;

    int has_slash = (dlen > 0 && dir[dlen - 1] == '/');

    while (dir[i])
    {
        if (i >= outsz - 1) return CPIO_ERR_PATH;

        out[i] = dir[i];
        i++;
    }
    if (!has_slash)
    {
        if (i >= outsz - 1) return CPIO_ERR_PATH;

        out[i++] = '/';
    }
    while (name[j])
    {
        if (i >= outsz - 1) return CPIO_ERR_PATH;

        out[i++] = name[j++];
    }

    out[i] = '\0';

    return CPIO_OK;
}

static int path_is_safe(const char *name)
{
    if (!name || !name[0]) return 0;
    if (name[0] == '/') return 0;

    const char *p = name;
    while (*p)
    {
        if (
            p[0] == '.' && p[1] == '.' &&
            (p == name || p[-1] == '/') &&
            (p[2] == '\0' || p[2] == '/')
        ) {
            return 0;
        }

        p++;
    }

    return 1;
}

static int write_all(
    int fd,
    const unsigned char *data,
    unsigned long size,
    unsigned long *written_out
) {
    unsigned long written = 0;

    while (written < size)
    {
        long result = write(
            fd,
            data + written,
            (size_t)(size - written)
        );

        if (result <= 0)
        {
            if (written_out) *written_out = written;

            return CPIO_ERR_WRITE;
        }

        written += (unsigned long)result;
    }

    if (written_out) *written_out = written;

    return CPIO_OK;
}

int cpio_extract_mem(
    const void *archive,
    unsigned long size,
    const char *dest_dir,
    cpio_extract_stats_t *out_stats
) {
    cpio_extract_stats_t local_stats;
    cpio_extract_stats_t *stats = out_stats ? out_stats : &local_stats;

    stats->total_entries = 0;
    stats->files_written = 0;
    stats->dirs_created = 0;

    if (!archive || !dest_dir) return CPIO_ERR_ARGUMENT;
    if (size < sizeof(cpio_header_t)) return CPIO_ERR_FORMAT;

    const unsigned char *base = (const unsigned char *)archive;
    unsigned long off = 0;

    if (mkdir(dest_dir, 0) < 0)
    {
        int fd = (int)open(dest_dir, O_RDONLY);
        if (fd < 0) return CPIO_ERR_IO;

        close(fd);
    }

    while (off < size)
    {
        const cpio_header_t *hdr = (const cpio_header_t *)(base + off);

        if (size - off < sizeof(cpio_header_t)) return CPIO_ERR_FORMAT;
        if (!magic_ok(hdr->magic)) return CPIO_ERR_FORMAT;

        unsigned int namesize = hex_val(hdr->namesize, 8);
        unsigned int filesize = hex_val(hdr->filesize, 8);
        unsigned int mode = hex_val(hdr->mode, 8);

        if (namesize == 0) return CPIO_ERR_FORMAT;

        unsigned long header_end = off + sizeof(cpio_header_t);
        const char *name = (const char *)(base + header_end);

        if (namesize > size - header_end) return CPIO_ERR_FORMAT;
        if (name[namesize - 1] != '\0') return CPIO_ERR_FORMAT;

        unsigned long data_off = align4(header_end + namesize);
        unsigned long next_off = align4(data_off + filesize);

        if (data_off > size) return CPIO_ERR_FORMAT;
        if (filesize > size - data_off) return CPIO_ERR_FORMAT;
        if (next_off > size) return CPIO_ERR_FORMAT;
        if (strcmp(name, CPIO_TRAILER) == 0) return CPIO_OK;

        const char *entry_name = name;

        if (entry_name[0] == '.' && entry_name[1] == '/')
        {
            entry_name += 2;
        }

        if (entry_name[0] == '\0' || strcmp(entry_name, ".") == 0)
        {
            off = next_off;
            continue;
        }

        if (!path_is_safe(entry_name)) return CPIO_ERR_PATH;

        char full_path[CPIO_PATH_MAX];

        int path_rc = join_path(
            full_path,
            sizeof(full_path),
            dest_dir,
            entry_name
        );

        if (path_rc != CPIO_OK) return path_rc;

        unsigned int type = mode & CPIO_TYPE_MASK;

        if (type == CPIO_TYPE_DIR)
        {
            if (mkdir(full_path, 0) < 0)
            {
                int fd = (int)open(
                    full_path,
                    O_RDONLY
                );

                if (fd < 0) return CPIO_ERR_IO;

                close(fd);
            }

            stats->dirs_created++;
        }
        else if (type == CPIO_TYPE_REG)
        {
            int fd = (int)open(
                full_path,
                O_WRONLY | O_CREAT | O_TRUNC
            );

            if (fd < 0) return CPIO_ERR_IO;

            unsigned long written = 0;

            int write_rc = write_all(
                fd,
                base + data_off,
                filesize,
                &written
            );

            close(fd);

            if (write_rc != CPIO_OK) return write_rc;
            if (written != filesize) return CPIO_ERR_WRITE;

            stats->files_written++;
        }

        stats->total_entries++;
        off = next_off;
    }

    return CPIO_ERR_FORMAT;
}

int cpio_extract_file(
    const char *cpio_path,
    const char *dest_dir,
    cpio_extract_stats_t *out_stats
) {
    if (!cpio_path || !dest_dir) return CPIO_ERR_ARGUMENT;

    int fd = (int)open(cpio_path, O_RDONLY);
    if (fd < 0) return CPIO_ERR_IO;

    long fsize = lseek(fd, 0, SEEK_END);
    if (fsize <= 0)
    {
        close(fd);
        return CPIO_ERR_FORMAT;
    }

    if (lseek(fd, 0, SEEK_SET) < 0)
    {
        close(fd);
        return CPIO_ERR_IO;
    }

    void *buf = malloc((size_t)fsize);
    if (!buf)
    {
        close(fd);
        return CPIO_ERR_IO;
    }

    long total_read = 0;
    while (total_read < fsize)
    {
        long r = read(fd, (char *)buf + total_read, (size_t)(fsize - total_read));
        if (r <= 0)
        {
            free(buf);
            close(fd);
            return CPIO_ERR_IO;
        }

        total_read += r;
    }
    close(fd);

    if (total_read != fsize)
    {
        free(buf);
        return CPIO_ERR_IO;
    }

    int rc = cpio_extract_mem(buf, (unsigned long)fsize, dest_dir, out_stats);
    free(buf);

    return rc;
}