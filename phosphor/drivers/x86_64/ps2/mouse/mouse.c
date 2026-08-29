/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: mouse.c
 *
 */


#include "mouse.h"
#include <kernel/arch/hal/ports.h>
#include <kernel/arch/x86_64/exceptions/irq.h>
#include <kernel/devices/input/ctrl.h>

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64
#define PS2_TIMEOUT 200000

static u8 packet[3];
static u8 packet_index;
static u8 buttons;

static int wait_input_clear(void)
{
    for (u32 i = 0; i < PS2_TIMEOUT; i++) {
        if (!(inb(PS2_STATUS) & 0x02)) return 1;
        io_wait();
    }
    return 0;
}

static int controller_command(u8 value)
{
    if (!wait_input_clear()) return 0;
    outb(PS2_COMMAND, value);
    io_wait();
    return 1;
}

static void drain_output(void)
{
    for (u32 i = 0; i < 256 && (inb(PS2_STATUS) & 0x01); i++) {
        (void)inb(PS2_DATA);
        io_wait();
    }
}

static int read_controller_byte(u8 *value)
{
    for (u32 i = 0; i < PS2_TIMEOUT; i++) {
        if (inb(PS2_STATUS) & 0x01) {
            *value = inb(PS2_DATA);
            return 1;
        }
        io_wait();
    }
    return 0;
}

static int write_controller_config(u8 config)
{
    if (!controller_command(0x60) || !wait_input_clear()) return 0;
    outb(PS2_DATA, config);
    io_wait();
    return 1;
}

static int read_mouse_reply(u8 *reply)
{
    for (u32 i = 0; i < PS2_TIMEOUT; i++) {
        u8 status = inb(PS2_STATUS);
        if (status & 0x01) {
            u8 data = inb(PS2_DATA);
            if (status & 0x20) {
                *reply = data;
                return 1;
            }
            /* Ignore an unrelated keyboard byte while polling the mouse. */
        }
        io_wait();
    }
    return 0;
}

static int write_mouse(u8 value)
{
    for (int attempt = 0; attempt < 3; attempt++) {
        if (!controller_command(0xD4) || !wait_input_clear()) return 0;
        outb(PS2_DATA, value);
        io_wait();

        u8 reply;
        if (!read_mouse_reply(&reply)) return 0;
        if (reply == 0xFA) return 1;
        if (reply != 0xFE) return 0;
    }
    return 0;
}

static void report_button(u8 changed, u8 bit, u16 code)
{
    if (changed & bit) input_report_key(code, !!(buttons & bit), 0);
}

static void mouse_irq_handler(cpu_state_t *state)
{
    (void)state;
    u8 status = inb(PS2_STATUS);
    if (!(status & 0x01) || !(status & 0x20)) return;

    u8 data = inb(PS2_DATA);
    if (packet_index == 0 && !(data & 0x08)) return;
    packet[packet_index++] = data;
    if (packet_index != 3) return;
    packet_index = 0;

    if (packet[0] & 0xC0) return;
    i32 dx = (i8)packet[1];
    i32 dy = -(i8)packet[2];
    if (dx) input_report_rel(INPUT_REL_X, dx);
    if (dy) input_report_rel(INPUT_REL_Y, dy);

    u8 old_buttons = buttons;
    buttons = packet[0] & 0x07;
    u8 changed = old_buttons ^ buttons;
    report_button(changed, 0x01, INPUT_BTN_LEFT);
    report_button(changed, 0x02, INPUT_BTN_RIGHT);
    report_button(changed, 0x04, INPUT_BTN_MIDDLE);
}

int ps2_mouse_init(void)
{
    packet_index = buttons = 0;

    /* Configure with both ports quiet so command replies cannot be confused. */
    if (!controller_command(0xAD) || !controller_command(0xA7)) return -1;
    drain_output();

    if (!controller_command(0x20)) return -2;
    u8 config;
    if (!read_controller_byte(&config)) return -2;
    config &= (u8)~0x03; /* IRQ1/IRQ12 stay off during device commands. */
    if (!write_controller_config(config)) return -3;

    if (!controller_command(0xA8)) return -4;
    if (!write_mouse(0xF6)) {
        controller_command(0xAE);
        return -5;
    }
    if (!write_mouse(0xF4)) {
        controller_command(0xAE);
        return -6;
    }

    irq_register_handler(12, mouse_irq_handler);

    /* Re-enable both clocks and both interrupt lines. */
    config |= 0x03;
    config &= (u8)~0x30;
    if (!write_controller_config(config)) return -7;
    if (!controller_command(0xAE) || !controller_command(0xA8)) return -8;
    return 0;
}

void ps2_mouse_fini(void)
{
    write_mouse(0xF5);
    irq_unregister_handler(12);
}
