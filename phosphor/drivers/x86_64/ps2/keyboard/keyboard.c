/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: keyboard.c
 *
 */

#include "keyboard.h"
#include <kernel/arch/hal/ports.h>
#include <kernel/arch/x86_64/exceptions/irq.h>
#include <kernel/devices/input/ctrl.h>
#include <kernel/devices/input/keycodes.h>

#define PS2_DATA    0x60
#define PS2_STATUS  0x64

#define SC_LSHIFT   0x2A
#define SC_RSHIFT   0x36
#define SC_CAPS     0x3A
#define SC_LCTRL    0x1D
#define SC_LALT     0x38
#define SC_RELEASE  0x80
#define SC_EXTENDED 0xE0

static const u8 sc_to_key[128] = {
    [0x01]=KEY_ESC,
    [0x02]=KEY_1, [0x03]=KEY_2, [0x04]=KEY_3, [0x05]=KEY_4, [0x06]=KEY_5,
    [0x07]=KEY_6, [0x08]=KEY_7, [0x09]=KEY_8, [0x0A]=KEY_9, [0x0B]=KEY_0,
    [0x0C]=KEY_MINUS, [0x0D]=KEY_EQUAL, [0x0E]=KEY_BACKSPACE, [0x0F]=KEY_TAB,
    [0x10]=KEY_Q, [0x11]=KEY_W, [0x12]=KEY_E, [0x13]=KEY_R, [0x14]=KEY_T,
    [0x15]=KEY_Y, [0x16]=KEY_U, [0x17]=KEY_I, [0x18]=KEY_O, [0x19]=KEY_P,
    [0x1A]=KEY_LBRACKET, [0x1B]=KEY_RBRACKET, [0x1C]=KEY_ENTER, [0x1D]=KEY_LCTRL,
    [0x1E]=KEY_A, [0x1F]=KEY_S, [0x20]=KEY_D, [0x21]=KEY_F, [0x22]=KEY_G,
    [0x23]=KEY_H, [0x24]=KEY_J, [0x25]=KEY_K, [0x26]=KEY_L,
    [0x27]=KEY_SEMICOLON, [0x28]=KEY_APOSTROPHE, [0x29]=KEY_GRAVE,
    [0x2A]=KEY_LSHIFT, [0x2B]=KEY_BACKSLASH,
    [0x2C]=KEY_Z, [0x2D]=KEY_X, [0x2E]=KEY_C, [0x2F]=KEY_V, [0x30]=KEY_B,
    [0x31]=KEY_N, [0x32]=KEY_M,
    [0x33]=KEY_COMMA, [0x34]=KEY_DOT, [0x35]=KEY_SLASH, [0x36]=KEY_RSHIFT,
    [0x37]=KEY_KP_ASTERISK, [0x38]=KEY_LALT, [0x39]=KEY_SPACE, [0x3A]=KEY_CAPSLOCK,
    [0x3B]=KEY_F1, [0x3C]=KEY_F2, [0x3D]=KEY_F3, [0x3E]=KEY_F4,
    [0x3F]=KEY_F5, [0x40]=KEY_F6, [0x41]=KEY_F7, [0x42]=KEY_F8,
    [0x43]=KEY_F9, [0x44]=KEY_F10, [0x57]=KEY_F11, [0x58]=KEY_F12,
};

static u8 extended_to_key(u8 make)
{
    switch (make) {
        case 0x1C: return KEY_KP_ENTER;
        case 0x1D: return KEY_RCTRL;
        case 0x35: return KEY_KP_SLASH;
        case 0x38: return KEY_RALT;
        case 0x47: return KEY_HOME;
        case 0x48: return KEY_UP;
        case 0x49: return KEY_PAGEUP;
        case 0x4B: return KEY_LEFT;
        case 0x4D: return KEY_RIGHT;
        case 0x4F: return KEY_END;
        case 0x50: return KEY_DOWN;
        case 0x51: return KEY_PAGEDOWN;
        case 0x52: return KEY_INSERT;
        case 0x53: return KEY_DELETE;
        default: return KEY_NONE;
    }
}

static volatile int shift_held   = 0;
static volatile int ctrl_held = 0;
static volatile int alt_held  = 0;
static volatile int caps_lock    = 0;
static volatile int extended_key = 0;

static u8 build_modifiers(void)
{
    u8 mods = 0;
    if (shift_held) mods |= INPUT_MOD_SHIFT;
    if (ctrl_held)  mods |= INPUT_MOD_CTRL;
    if (alt_held)   mods |= INPUT_MOD_ALT;
    if (caps_lock)  mods |= INPUT_MOD_CAPS;
    return mods;
}

static void keyboard_irq_handler(cpu_state_t *state)
{
    (void)state;

    if (!(inb(PS2_STATUS) & 1))
        return;

    u8 sc = inb(PS2_DATA);

    if (sc == SC_EXTENDED) {
        extended_key = 1;
        return;
    }

    u8 pressed = !(sc & SC_RELEASE);
    u8 make    = sc & ~SC_RELEASE;

    if (extended_key) {
        extended_key = 0;
        u8 code = extended_to_key(make);
        if (code == KEY_RCTRL) ctrl_held = pressed;
        if (code == KEY_RALT) alt_held = pressed;
        if (code != KEY_NONE)
            input_report_key(code, pressed, build_modifiers());
        return;
    }

    if (
    	make == SC_LSHIFT ||
     	make == SC_RSHIFT) shift_held = pressed;
    else if (make == SC_LCTRL) ctrl_held  = pressed;
    else if (make == SC_LALT) alt_held = pressed;
    else if (
    	make == SC_CAPS &&
    	pressed
    ) caps_lock  = !caps_lock;

    if (make >= 128) return;

    u8 code = sc_to_key[make];
    if (code == KEY_NONE) return;

    input_report_key(code, pressed, build_modifiers());
}

void keyboard_init(void)
{
    shift_held = 0;
    ctrl_held  = 0;
    alt_held   = 0;
    caps_lock  = 0;
    extended_key = 0;

    while (inb(PS2_STATUS) & 1) inb(PS2_DATA);

    irq_register_handler(1, keyboard_irq_handler);
}

void keyboard_fini(void)
{
    irq_unregister_handler(1);
}
