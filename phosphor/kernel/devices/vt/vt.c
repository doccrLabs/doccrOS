/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: vt.c
 *
 */

#include "vt.h"
#include "../names.h"
#include <kernel/fs/vfs/vfs.h>
#include <kernel/fs/devfs/devfs.h>
#include <kernel/mem/meminclude.h>
#include <kernel/screen/lib/print.h>
#include <kernel/screen/lib/string.h>
#include <kernel/screen/colors.h>
#include <kernel/screen/lib/log.h>
#include <kernel/screen/bootscreen/boot.h>
#include <kernel/devices/input/keycodes.h>
#include <kernel/devices/input/input.h>
#include <kernel/communication/serial.h>

// fbcon(bs3) is normaly on but desktop needs to deactivate
// it so the scrolling doesnt ruin the rendering of the desktop
static int g_vt_enabled = 1;
/*
void vt_set_enabled(int enabled) // replacement for drm for desktop
{
    g_vt_enabled = enabled ? 1 : 0;

    log(
        "[VT]",
        g_vt_enabled ? "fbcon(bs3) output enabled\n" : "fbcon(bs3) output disabled \n",
        d
    );
}*/

typedef struct
{
    volatile char buf[VT_QUEUE_SIZE];
    volatile u32 head;
    volatile u32 tail;
} vt_ring_t;

typedef struct
{
    int used;
    int refcount;
    u64 id;
    vt_ring_t input;
    vt_ring_t output;
} vt_t;

typedef struct
{
    vt_t *vt;
} vt_handle_t;

static int g_screen_enabled = 1;

int vt_screen_enabled(void)
{
    return g_screen_enabled;
}
void vt_screen_set_enabled(int enabled)
{
    g_screen_enabled = enabled ? 1 : 0;
}

static vt_t g_vts[VT_MAX];
static u64 g_next_id = 1;
static vt_t *g_active_vt = NULL;

static void ring_init(vt_ring_t *r)
{
    r->head = 0;
    r->tail = 0;
}
static int ring_push(vt_ring_t *r, char c)
{
    u32 next = (r->head + 1) % VT_QUEUE_SIZE;
    if (next == r->tail) return 0; //full

    r->buf[r->head] = c;
    r->head = next;
    return 1;
}
static int ring_pop(vt_ring_t *r, char *out)
{
    if (r->head == r->tail) return 0;

    *out = r->buf[r->tail];
    r->tail = (r->tail + 1) % VT_QUEUE_SIZE;
    return 1;
}


static vt_t *vt_find(u64 id)
{
    for (int i = 0; i < VT_MAX; i++)
    {
        if (g_vts[i].used && g_vts[i].id == id) return &g_vts[i];
    }
    return NULL;
}

static void vt_path(u64 id, char out[VT_PATH_MAX])
{
    char num[20];
    char rev[20];
    int i = 0;
    int j = 0;
    u64 v = id;

    str_copy(out, VT_ID_MOUNT);

    if (v == 0)
    {
        str_append(out, "0");
        return;
    }

    while (v)
    {
        num[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (i > 0) rev[j++] = num[--i];
    rev[j] = '\0';

    str_append(out, rev);
}

static vt_t *vt_alloc(void)
{
    for (int i = 0; i < VT_MAX; i++)
    {
        if (!g_vts[i].used)
        {
            g_vts[i].used = 1;
            g_vts[i].id = g_next_id++;
            g_vts[i].refcount = 0;

            ring_init(&g_vts[i].input);
            ring_init(&g_vts[i].output);
            return &g_vts[i];
        }
    }
    return NULL;
}
static void vt_free(vt_t *vt)
{
    if (g_active_vt == vt) g_active_vt = NULL;

    char path[VT_PATH_MAX];

    vt_path(vt->id, path);
    vfs_remove(path);

    vt->used = 0;
    vt->refcount = 0;
    vt->id = 0;
}

//
// id handling
// /dev/vt/<id>
//
// through ioctl you get your terminal ID and can then connect (just write to it)

static u64 path_to_id(const char *path)
{
    const char *base = path;
    for (const char *s = path; *s; s++)  if (*s == '/') base = s + 1;

    const char *p = base;
    u64 v = 0;

    while (*p && (*p < '0' || *p > '9')) p++;

    while (*p >= '0' && *p <= '9')
    {
        v = v * 10 + (u64)(*p - '0');
        p++;
    }

    return v;
}

static char vt_keycode_to_ascii(u16 code, u8 modifiers)
{
    int shift = (modifiers & INPUT_MOD_SHIFT) != 0;

    switch ((keycode_t)code)
    {
        case KEY_A: return shift ? 'A' : 'a';
        case KEY_B: return shift ? 'B' : 'b';
        case KEY_C: return shift ? 'C' : 'c';
        case KEY_D: return shift ? 'D' : 'd';
        case KEY_E: return shift ? 'E' : 'e';
        case KEY_F: return shift ? 'F' : 'f';
        case KEY_G: return shift ? 'G' : 'g';
        case KEY_H: return shift ? 'H' : 'h';
        case KEY_I: return shift ? 'I' : 'i';
        case KEY_J: return shift ? 'J' : 'j';
        case KEY_K: return shift ? 'K' : 'k';
        case KEY_L: return shift ? 'L' : 'l';
        case KEY_M: return shift ? 'M' : 'm';
        case KEY_N: return shift ? 'N' : 'n';
        case KEY_O: return shift ? 'O' : 'o';
        case KEY_P: return shift ? 'P' : 'p';
        case KEY_Q: return shift ? 'Q' : 'q';
        case KEY_R: return shift ? 'R' : 'r';
        case KEY_S: return shift ? 'S' : 's';
        case KEY_T: return shift ? 'T' : 't';
        case KEY_U: return shift ? 'U' : 'u';
        case KEY_V: return shift ? 'V' : 'v';
        case KEY_W: return shift ? 'W' : 'w';
        case KEY_X: return shift ? 'X' : 'x';
        case KEY_Y: return shift ? 'Y' : 'y';
        case KEY_Z: return shift ? 'Z' : 'z';

        case KEY_0: return shift ? ')' : '0';
        case KEY_1: return shift ? '!' : '1';
        case KEY_2: return shift ? '@' : '2';
        case KEY_3: return shift ? '#' : '3';
        case KEY_4: return shift ? '$' : '4';
        case KEY_5: return shift ? '%' : '5';
        case KEY_6: return shift ? '^' : '6';
        case KEY_7: return shift ? '&' : '7';
        case KEY_8: return shift ? '*' : '8';
        case KEY_9: return shift ? '(' : '9';

        case KEY_SPACE: return ' ';
        case KEY_ENTER: return '\n';
        case KEY_KP_ENTER: return '\n';
        case KEY_BACKSPACE: return '\b';
        case KEY_TAB: return '\t';

        case KEY_MINUS:      return shift ? '_' : '-';
        case KEY_EQUAL:      return shift ? '+' : '=';
        case KEY_LBRACKET:   return shift ? '{' : '[';
        case KEY_RBRACKET:   return shift ? '}' : ']';
        case KEY_BACKSLASH:  return shift ? '|' : '\\';
        case KEY_SEMICOLON:  return shift ? ':' : ';';
        case KEY_APOSTROPHE: return shift ? '"' : '\'';
        case KEY_GRAVE:      return shift ? '~' : '`';
        case KEY_COMMA:      return shift ? '<' : ',';
        case KEY_DOT:        return shift ? '>' : '.';
        case KEY_SLASH:      return shift ? '?' : '/';

        default: return 0;
    }
}

void vt_kbd_feed(u16 code, u8 modifiers)
{
    if (!g_active_vt) return;

    char c = vt_keycode_to_ascii(code, modifiers);
    if (!c) return;

    serial_putchar(c);

    ring_push(&g_active_vt->input, c);
}

static void *vt_id_open(const char *path)
{
    u64 id = path_to_id(path);
    vt_t *vt = vt_find(id);
    vt_handle_t *h = (vt_handle_t *)kmalloc(sizeof(vt_handle_t));

    if (!vt) return NULL;
    if (!h) return NULL;

    h->vt = vt;
    vt->refcount++;

    return h;
}

static void vt_id_close(void *handle)
{
    vt_handle_t *h = (vt_handle_t *)handle;

    if (h->vt) h->vt->refcount--; // dies when calling destroy with ioctl

    //kfree((u64*)h);
}

static int vt_id_read(void *handle, void *buf, size_t count)
{
    vt_handle_t *h = (vt_handle_t *)handle;
    if (!h || !h->vt || !buf) return -1;

    u8 *dst = (u8 *)buf;
    size_t written = 0;
    char c;

    while (written < count && ring_pop(&h->vt->input, &c))
    {
        dst[written++] = (u8)c;
    }

    return (int)written;
}

static int vt_id_write(void *handle, const void *buf, size_t count)
{
    vt_handle_t *h = (vt_handle_t *)handle;
    if (!h || !h->vt || !buf) return -1;

    const char *src = (const char *)buf;

    serial_printf("[VT%llu] ", h->vt->id);
    for (size_t i = 0; i < count; i++) serial_putchar(src[i]);
    //oring
    for (size_t i = 0; i < count; i++) ring_push(&h->vt->output, src[i]);

    //TODO:
    // everything into one buffer
    // and then load the screen when active
    if (h->vt == g_active_vt && g_screen_enabled)
    {
        for (size_t i = 0; i < count; i++) putchar(src[i], white());
    }

    return (int)count;
}

static i64 vt_id_ioctl(void *handle, u64 request, void *arg)
{
    vt_handle_t *h = (vt_handle_t *)handle;

    switch (request)
    {
        case VT_IOCTL_GET_ID:
        {
            *(u64 *)arg = h->vt->id;
            return 0;
        }

        case VT_IOCTL_ACTIVATE:
        {
            g_active_vt = h->vt;
            return 0;
        }

        case VT_IOCTL_DEACTIVATE:
        {
            if (g_active_vt == h->vt) g_active_vt = NULL;
            return 0;
        }

        case VT_IOCTL_FEED:
        {
            if (!arg) return -1;

            vt_feed_args_t args = *(vt_feed_args_t *)arg;
            if (!args.data) return -1;

            const char *src = (const char *)args.data;
            for (u64 i = 0; i < args.len; i++) ring_push(&h->vt->input, src[i]);

            return 0;
        }

        case VT_IOCTL_CLEAR:
        {
            if (h->vt == g_active_vt) bs.Clear(BS3);
            return 0;
        }

        case VT_IOCTL_READ_OUTPUT:
        {
            if (!arg) return -1;

            vt_drain_args_t args = *(vt_drain_args_t *)arg;
            if (!args.data) return -1;

            char *dst = (char *)args.data;
            u64 written = 0;
            char c;

            while (written < args.len && ring_pop(&h->vt->output, &c))
            {
                dst[written++] = c;
            }

            args.len = written;
            *(vt_drain_args_t *)arg = args;

            return (i64)written;
        }

        default:
            return -1;
    }
}

static device_handler vt_id_device_ops =
{
    .name    = "vt_istc",
    .mount   = NULL, //mounted per ID's
    .version = VERSION_NUM(0, 0, 0, 0),
    .init    = NULL,
    .fini    = NULL,
    .open    = vt_id_open,
    .close   = vt_id_close,
    .read    = vt_id_read,
    .write   = vt_id_write,
    .ioctl   = vt_id_ioctl,
};

//
// /dev/vt/ctl
// cuz ioctl needs to send commands to vt

// its literally just for ioctl available btw

static void *vt_ctl_open(const char *path)
{
    (void)path;
    return (void *)1;
}

static void vt_ctl_close(void *handle)
{
    (void)handle;
}

static i64 vt_ctl_ioctl(void *handle, u64 request, void *arg)
{
    (void)handle;

    switch (request)
    {
        case VT_IOCTL_CREATE:
        {
            vt_t *vt = vt_alloc();
            char path[VT_PATH_MAX];

            if (!arg) return -1;
            if (!vt) return -1;

            vt_path(vt->id, path);

            vfs_node_t *node = devfs_mount(path, &vt_id_device_ops);
            if (!node)
            {
                vt->used = 0;
                return -1;
            }

            #if DEBUGINFO == 1
                printf("[VT] created %s\n", path);
            #endif

            *(u64 *)arg = vt->id;

            return 0;
        }

        case VT_IOCTL_DESTROY:
        {
            if (!arg) return -1;

            vt_t *vt = vt_find(*(u64 *)arg);
            if (!vt) return -1;

            vt_free(vt);
            #if DEBUGINFO == 1
                printf("[VT] destroyed %s\n", &vt);
            #endif

            return 0;
        }

        case VT_IOCTL_GET_ACTIVE:
        {
            if (!arg) return -1;
            *(u64 *)arg = g_active_vt ? g_active_vt->id : 0;
            return 0;
        }

        case VT_IOCTL_SET_ACTIVE:
        {
            if (!arg) return -1;

            u64 id = *(u64 *)arg;
            if (id == 0)
            {
                g_active_vt = NULL;
                return 0;
            }

            vt_t *vt = vt_find(id);
            if (!vt) return -1;

            g_active_vt = vt;
            return 0;
        }

        default:
            return -1;
    }
}

static int vt_ctl_dev_init(void)
{
    for (int i = 0; i < VT_MAX; i++) g_vts[i].used = 0;

    g_next_id = 1;
    g_active_vt = NULL;
    g_screen_enabled = 1;

    log("[VT]", "ctldev ready\n");
    return 0;
}

device_handler vt_ctl_device =
{
    .name    = VT_CTL_NAME,
    .mount   = VT_CTL_MOUNT,
    .version = VT_CTL_VERSION,
    .init    = vt_ctl_dev_init,
    .fini    = NULL,
    .open    = vt_ctl_open,
    .close   = vt_ctl_close,
    .read    = NULL,
    .write   = NULL,
    .ioctl   = vt_ctl_ioctl,
};

void vt_manager_init(void)
{
    device_register(&vt_ctl_device);
}