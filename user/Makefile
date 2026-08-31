#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Copyright (c) 2026 sulfurLabs
#
# PROJECT: sulfurOS
# FILE: Makefile
#

all:
	@$(MAKE) -C libc

	@$(MAKE) -C libs/libcpio
	@$(MAKE) -C libs/tinygl

	@$(MAKE) -C bin/poweroff
	@$(MAKE) -C bin/reboot
#@$(MAKE) -C bin/doomgeneric ## default doom
	@$(MAKE) -C bin/hello
	@$(MAKE) -C bin/system

	@$(MAKE) -C apps/template
	@$(MAKE) -C apps/welcome
	@$(MAKE) -C apps/login
	@$(MAKE) -C apps/gears
	@$(MAKE) -C apps/doomgeneric ## desktop doom
	@$(MAKE) -C apps/terminal

clean:
	@$(MAKE) -C libc clean

	@$(MAKE) -C libs/libcpio clean
	@$(MAKE) -C libs/tinygl clean

	@$(MAKE) -C bin/poweroff clean
	@$(MAKE) -C bin/reboot clean
#@$(MAKE) -C bin/doomgeneric clean
	@$(MAKE) -C bin/system clean
	@$(MAKE) -C bin/hello clean

	@$(MAKE) -C apps/template clean
	@$(MAKE) -C apps/welcome clean
	@$(MAKE) -C apps/login clean
	@$(MAKE) -C apps/gears clean
#@$(MAKE) -C apps/doomgeneric clean
	@$(MAKE) -C apps/terminal clean

.PHONY: all clean
