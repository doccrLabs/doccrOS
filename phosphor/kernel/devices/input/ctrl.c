/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: ctrl.c
 *
 */

#include "ctrl.h"
#include "kbd.h"
#include "mouse.h"
#include <kernel/screen/lib/log.h>
#include <kernel/devices/vt/vt.h>

void input_ctrl_init(void)
{
    log("[INPUT]", "init input controller\n");
}

void input_report_key(u16 code, int pressed, u8 modifiers)
{
    input_event_t ev;
    ev.type  = INPUT_EV_KEY;
    ev.code  = code;
    ev.value = pressed ? 1 : 0;
    ev.modifiers = modifiers;

    input_dispatch(&ev);
}

void input_report_rel(u16 axis, i32 value)
{
    input_event_t ev = {
        .type = INPUT_EV_REL,
        .code = axis,
        .value = value,
        .modifiers = 0,
    };
    input_dispatch(&ev);
}

void input_dispatch(const input_event_t *ev)
{
    switch (ev->type)
    {
        case INPUT_EV_KEY:
            if (ev->code >= INPUT_BTN_LEFT && ev->code <= INPUT_BTN_MIDDLE)
            {
                mouse_push_event(ev);
            } else
            {
                kbd_push_event(ev);
                if (ev->value != 0) vt_kbd_feed(ev->code, ev->modifiers);
            }

            break;

        case INPUT_EV_REL:
            mouse_push_event(ev);
            break;
        case INPUT_EV_ABS:
        default:
            break;
    }
}
