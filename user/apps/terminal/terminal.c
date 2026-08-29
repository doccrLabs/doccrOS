/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: terminal.c
 *
 */

#include <ui16.h>
#include <ui16buttons.h>

#include <libdesktop.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/input.h>

#include <sys/vt.h>

#define WIN_W 640
#define WIN_H 400

#define TERM_COLS 78
#define TERM_ROWS_VISIBLE 30
#define TERM_SCROLLBACK 400

static unsigned int *window_buffer;
static int vt_fd = -1;

static char term_buf[TERM_SCROLLBACK][TERM_COLS + 1];
static int term_line_len[TERM_SCROLLBACK];
static int term_total_lines = 1;
static int term_cur_col = 0;

static void term_clear(void)
{
    term_total_lines = 1;
    term_cur_col = 0;
    term_buf[0][0] = '\0';
    term_line_len[0] = 0;
}

static void term_newline(void)
{
    term_buf[term_total_lines - 1][term_cur_col] = '\0';
    term_line_len[term_total_lines - 1] = term_cur_col;

    if (term_total_lines >= TERM_SCROLLBACK)
    {
        for (int i = 1; i < TERM_SCROLLBACK; i++)
        {
            memcpy(term_buf[i - 1], term_buf[i], TERM_COLS + 1);
            term_line_len[i - 1] = term_line_len[i];
        }
        term_total_lines = TERM_SCROLLBACK - 1;
    }

    term_total_lines++;
    term_cur_col = 0;
    term_buf[term_total_lines - 1][0] = '\0';
    term_line_len[term_total_lines - 1] = 0;
}

static void term_putc(char c)
{
    if (c == '\n')
    {
        term_newline();
        return;
    }

    if (c == '\f')
    {
        term_clear();
        return;
    }

    if (c == '\b')
    {
        if (term_cur_col > 0)
        {
            term_cur_col--;
            term_buf[term_total_lines - 1][term_cur_col] = '\0';
        }
        return;
    }

    if (c == '\t')
    {
        for (int i = 0; i < 4; i++) term_putc(' ');
        return;
    }

    if ((unsigned char)c < 32) return;

    if (term_cur_col >= TERM_COLS) term_newline();

    term_buf[term_total_lines - 1][term_cur_col++] = c;
    term_buf[term_total_lines - 1][term_cur_col] = '\0';
}

#define DRAIN_CHUNK 256

static void term_pump_output(void)
{
    char chunk[DRAIN_CHUNK];

    for (;;)
    {
        vt_drain_args_t args;
        args.data = chunk;
        args.len = DRAIN_CHUNK;

        if (ioctl(vt_fd, VT_IOCTL_READ_OUTPUT, &args) < 0) break;
        if (args.len == 0) break;

        for (unsigned long i = 0; i < args.len; i++) term_putc(chunk[i]);

        if (args.len < DRAIN_CHUNK) break;
    }
}

static void term_feed_char(char c)
{
    if (!c) return;

    vt_feed_args_t args;
    args.data = &c;
    args.len = 1;

    ioctl(vt_fd, VT_IOCTL_FEED, &args);
}

static void handle_key_event(dt_event_t *ev)
{
    if (ev->type != DT_EV_KEY || !ev->pressed) return;

    int shift = (ev->modifiers & INPUT_MOD_SHIFT) != 0;
    char c = ui16_keyToChar(ev->keycode, shift);

    term_feed_char(c);
}

static void draw_ui(void)
{
    // colors from ui16
    #define TERM_BG rgb(10, 10, 10)
    #define TERM_FG rgb(220, 220, 220)

    ui16_setRoot(
        style(
            width(fill),
            height(fill),
            bg( TERM_BG )
        ),
        window_buffer,

        WIN_W,
        WIN_H
    );

    ui16_container(
        style(
            width(fill),
            height(fill),
            layout(column),
            padding(6)
        )
    ) {
        int start = term_total_lines - TERM_ROWS_VISIBLE;
        if (start < 0) start = 0;

        for (int i = start; i < term_total_lines; i++)
        {
            const char *text = term_buf[i][0] ? term_buf[i] : " ";

            ui16_label(
                style(
                    font(fontRegular),
                    color( TERM_FG )
                ),

                text
            );
        }
    }

    ui16_frame();
    desktop.presentFrame();
}

int main(void)
{
    int ctl_fd = (int)open("/dev/vt/ctl", O_RDWR);
    if (ctl_fd < 0) return 1;

    unsigned long id = 0;
    if (ioctl(ctl_fd, VT_IOCTL_CREATE, &id) < 0)
    {
        close(ctl_fd);
        return 1;
    }
    close(ctl_fd);

    char vt_path[32];
    snprintf(vt_path, sizeof(vt_path), VT_DEV_PATH_FMT, (int)id);

    vt_fd = (int)open(vt_path, O_RDWR);
    if (vt_fd < 0) return 1;

    long pid = fork();
    if (pid < 0) return 1;

    if (pid == 0)
    {
        dup2(vt_fd, STDIN_FILENO);
        dup2(vt_fd, STDOUT_FILENO);
        dup2(vt_fd, STDERR_FILENO);
        close(vt_fd);

        char *argv[] = { "x1", "--no-vt", NULL };
        execve("/bin/x1.elf", argv, NULL);

        _exit(127);
    }

    term_clear();

    int window_width;
    int window_height;
    #define APP_TITLE "Terminal"

    desktopWindowSizeForContent(
        WIN_W,
        WIN_H,
        DT_WIN,

        &window_width,
        &window_height
    );

    desktop.createWindow(
        APP_TITLE,

        120,   90,
        window_width,
        window_height,

        DT_WIN
    );

    window_buffer = desktop.allocFramebuffer(WIN_W, WIN_H);

    for (;;)
    {
        draw_ui();

        dt_event_t incoming_events[16];
        int event_count = desktop.pollEvents(incoming_events, 16);

        for (int i = 0; i < event_count; i++)
        {
            handle_key_event(&incoming_events[i]);
        }

        term_pump_output();

        yield();
    }

    return 0;
}