/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: welcome.c
 *
 */

#include <ui16.h>
#include <ui16buttons.h>
#include <libdesktop.h>
#include <unistd.h>

#define APP_TITLE "Welcome"

#define WIN_W 500
#define WIN_H 250

#define BG_COLOR rgb(255, 255, 255)
#define FG_COLOR rgb(0, 0, 0)
#define ACCENT_COLOR rgb(0, 102, 204)

static unsigned int *window_buffer;

static void draw_ui(void)
{
    ui16_setRoot(
        style(
            width(fill),
            height(fill),
            bg(BG_COLOR)
        ),
        window_buffer,
        WIN_W,
        WIN_H
    );

    ui16_container(
        style(
            width(fill),
            height(fill),
            padding(20),
            layout(column),
            gap(8)
        )
    ) {
        ui16_label(
            style(
                font(fontPsf("/system/fonts/terminus/ter-powerline-v14b.psf")),
                color(FG_COLOR)
            ),
            "Welcome to sulfurOS!"
        );

        ui16_container(
            style(
                width(fill),
                height(1),
                bg(rgb(128, 128, 128))
            )
        );

        ui16_label(
            style(
                font(fontRegular),
                color(FG_COLOR)
            ),
            "sulfurOS is a small graphical operating system entirely"
        );

        ui16_label(
            style(
                font(fontRegular),
                color(FG_COLOR)
            ),
            "written from scratch in C by emex and all its contributors."
        );

        ui16_label(
            style(
                font(fontRegular),
                color(FG_COLOR)
            ),
            "It's designed for users who want to fully customize the"
        );

        ui16_label(
            style(
                font(fontRegular),
                color(FG_COLOR)
            ),
            "look of their system and love retro with a modern touch."
        );

        ui16_container(
            style(
                height(20)
            )
        );

        ui16_label(
            style(
                font(fontRegular),
                color(FG_COLOR)
            ),
            "Have fun exploring the system!"
        );

        ui16_container(
            style(
                width(fill),
                layout(row)
            )
        ) {
            ui16_container(
                style(
                    width(fill)
                )
            );

            ui16_label(
                style(
                    font(fontBold),
                    color(ACCENT_COLOR)
                ),
                "~sulfurLabs"
            );
        };
    }

    ui16_frame();
}

int main(void)
{
    int window_width;
    int window_height;

    desktopWindowSizeForContent(
        WIN_W,
        WIN_H,
        DT_WIN,
        &window_width,
        &window_height
    );

    int screen_w = 1280;
    int screen_h = 720;

    int win_x = (screen_w - window_width) / 2;
    int win_y = (screen_h - window_height) / 2;

    desktop.createWindow(
        APP_TITLE,
        win_x,
        win_y,
        window_width,
        window_height,
        DT_WIN
    );

    window_buffer = desktop.allocFramebuffer(
        WIN_W,
        WIN_H
    );

    for (;;)
    {
        draw_ui();

        desktop.presentFrame();

        dt_event_t incoming_events[8];
        desktop.pollEvents(incoming_events, 8);

        yield();
    }

    return 0;
}