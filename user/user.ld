/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: linker.ld
 *
 */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

ENTRY(_start)

PHDRS {
	text PT_LOAD FLAGS(5); /*  R+X   */
	data PT_LOAD FLAGS(6); /*  R+W   */
}

SECTIONS
{
    . = 0x40000000;

    .text :
    {
	    *(.text._start)
	    *(.text*)
	    *(.rodata*)
    } :text

    . = ALIGN(4096);

    .data :
    {
	    *(.data*)
    } :data

    .bss :
    {
	    *(.bss*)
	    *(COMMON)
    } :data

    /DISCARD/ :
    {
	    *(.eh_frame)
	    *(.note*)
	    *(.comment)
    }
}