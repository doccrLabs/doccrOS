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
	@$(MAKE) -C bin/template
	@$(MAKE) -C bin/welcome
	@$(MAKE) -C bin/system
	@$(MAKE) -C bin/login
	@$(MAKE) -C bin/hello
	@$(MAKE) -C bin/gears
#@$(MAKE) -C bin/doomgeneric

clean:
	@$(MAKE) -C libc clean

	@$(MAKE) -C libs/libcpio clean
	@$(MAKE) -C libs/tinygl clean

	@$(MAKE) -C bin/poweroff clean
	@$(MAKE) -C bin/reboot clean
	@$(MAKE) -C bin/template clean
	@$(MAKE) -C bin/welcome clean
	@$(MAKE) -C bin/system clean
	@$(MAKE) -C bin/login clean
	@$(MAKE) -C bin/hello clean
	@$(MAKE) -C bin/gears clean
#@$(MAKE) -C bin/doomgeneric clean

.PHONY: all clean
