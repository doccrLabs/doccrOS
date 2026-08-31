#include "doomgeneric.h"
#include "doomkeys.h"
#include <sys/input.h>
#include <libdesktop.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#ifndef DOCCROS_DOOM_SCALE
#   define DOCCROS_DOOM_SCALE 2
#endif

static unsigned int *s_shm = NULL;
static int s_content_w = DOOMGENERIC_RESX * DOCCROS_DOOM_SCALE;
static int s_content_h = DOOMGENERIC_RESY * DOCCROS_DOOM_SCALE;

static unsigned char keymap(uint16_t k)
{
    static const char rows[] = "1234567890-=\0\0qwertyuiop[]\0\0asdfghjkl;'`\0\\zxcvbnm,./";

    switch (k)
    {
        case INPUT_KEY_ESC:
            return KEY_ESCAPE;

        case INPUT_KEY_ENTER:
        case INPUT_KEY_KP_ENTER:
            return KEY_ENTER;

        case INPUT_KEY_TAB:
            return KEY_TAB;

        case INPUT_KEY_BACKSPACE:
            return KEY_BACKSPACE;

        case INPUT_KEY_LCTRL:
        case INPUT_KEY_RCTRL:
            return KEY_FIRE;

        case INPUT_KEY_LSHIFT:
        case INPUT_KEY_RSHIFT:
            return KEY_RSHIFT;

        case INPUT_KEY_LALT:
        case INPUT_KEY_RALT:
            return KEY_RALT;

        case INPUT_KEY_UP:
            return KEY_UPARROW;

        case INPUT_KEY_DOWN:
            return KEY_DOWNARROW;

        case INPUT_KEY_LEFT:
            return KEY_LEFTARROW;

        case INPUT_KEY_RIGHT:
            return KEY_RIGHTARROW;

        case INPUT_KEY_HOME:
            return KEY_HOME;

        case INPUT_KEY_END:
            return KEY_END;

        case INPUT_KEY_PAGEUP:
            return KEY_PGUP;

        case INPUT_KEY_PAGEDOWN:
            return KEY_PGDN;

        case INPUT_KEY_INSERT:
            return KEY_INS;

        case INPUT_KEY_DELETE:
            return KEY_DEL;

        case INPUT_KEY_F11:
            return KEY_F11;

        case INPUT_KEY_F12:
            return KEY_F12;

        case INPUT_KEY_SPACE:
            return KEY_USE;

        default:
            break;
    }

    if (k >= INPUT_KEY_F1 && k <= INPUT_KEY_F10) return (unsigned char)(KEY_F1 + k - INPUT_KEY_F1);
    if (k >= INPUT_KEY_1 && k <= INPUT_KEY_SLASH) return (unsigned char)rows[k - INPUT_KEY_1];

    return 0;
}

static void resize_framebuffer(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    unsigned int *new_buffer = desktop.resizeFramebuffer(width, height);

    if (new_buffer)
    {
        s_shm = new_buffer;
        s_content_w = width;
        s_content_h = height;
    }
}

static void handle_events(void)
{
    dt_event_t events[16];
    int count = desktop.pollEvents(events, 16);

    for (int i = 0; i < count; i++)
    {
        dt_event_t *ev = &events[i];

        if (ev->type == DT_EV_RESIZE)
        {
            if (ev->width > 40 && ev->height > 40) resize_framebuffer(ev->width, ev->height);
        }
    }
}

void DG_Init(void)
{
    if (!dt_check_abi())
    {
        char m[96];
        snprintf(
            m,
            sizeof(m),
            "[DOOM] libdesktop ABI mismatch (got %u, expected %d)\n",
            desktop.abi_version,
            DT_ABI_VERSION
        );

        write(1, m, strlen(m));
        exit(1);
    }

    int win_w;
    int win_h;

    desktopWindowSizeForContent(
        DOOMGENERIC_RESX * DOCCROS_DOOM_SCALE,
        DOOMGENERIC_RESY * DOCCROS_DOOM_SCALE,
        DT_WIN,
        &win_w,
        &win_h
    );

    int rc = desktop.createWindow(
        "DOOM",

        100,   100,
        win_w,
        win_h,

        DT_WIN
    );

    char msg[64];

    snprintf(
        msg,
        sizeof(msg),
        "[DOOM] createWindow rc=%d\n",
        rc
    );

    write(1, msg, strlen(msg));

    s_content_w = DOOMGENERIC_RESX * DOCCROS_DOOM_SCALE;
    s_content_h = DOOMGENERIC_RESY * DOCCROS_DOOM_SCALE;

    s_shm = desktop.allocFramebuffer(
        s_content_w,
        s_content_h
    );

    if (!s_shm)
    {
        write(1, "[DOOM] framebuffer alloc failed\n", 32);
        exit(1);
    }
}

void DG_DrawFrame(void)
{
    if (!s_shm) return;

    for (int y = 0; y < s_content_h; y++)
    {
        int src_y = (y * DOOMGENERIC_RESY) / s_content_h;

        unsigned int *dst = s_shm + y * s_content_w;
        unsigned int *src = DG_ScreenBuffer + src_y * DOOMGENERIC_RESX;

        for (int x = 0; x < s_content_w; x++)
        {
            int src_x = (x * DOOMGENERIC_RESX) / s_content_w;

            dst[x] = src[src_x];
        }
    }

    desktop.presentFrame();
}

uint32_t DG_GetTicksMs(void)
{
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);

    return (uint32_t)(
        t.tv_sec * 1000 +
        t.tv_nsec / 1000000
    );
}

void DG_SleepMs(uint32_t ms)
{
    uint32_t end = DG_GetTicksMs() + ms;

    while ((int32_t)(DG_GetTicksMs() - end) < 0) yield();
}

int DG_GetKey(int *pressed, unsigned char *key)
{
    input_event_t e;

    long n = read(0, &e, sizeof(e));

    *key = keymap(e.code);

    if (n != (long)sizeof(e)) return 0;
    if (e.type != INPUT_EV_KEY) return 0;
    if (!*key) return 0;

    *pressed = e.value != 0;

    return 1;
}

void DG_SetWindowTitle(const char *t)
{
    desktop.setTitle(t);
}

int main(void)
{
    char *argv[] =
        {
        "doomgeneric",
        "-iwad",
        "/doom1.wad",
        0
    };

    doomgeneric_Create(3, argv);

    for (;;)
    {
        handle_events();
        doomgeneric_Tick();
    }

    return 0;
}