CC := x86_64-elf-gcc
LD := x86_64-elf-ld
LIBC := ../../libc
LIBS := ../../libs
LIBDESKTOP_H := $(LIBS)/libdesktop/libdesktop.h
LIBC_HEADERS := $(wildcard $(LIBC)/include/*.h $(LIBC)/include/sys/*.h)
DOOMGENERIC_DIR ?= ../../../external/doomgeneric/doomgeneric
DOOM_SCALE ?= 2
CFLAGS := -ffreestanding -nostdlib -fno-builtin -fno-stack-protector -fno-PIE -fno-pic -m64 -march=x86-64 -mno-red-zone -O2 -DNORMALUNIX -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 -DDOCCROS_DOOM_SCALE=$(DOOM_SCALE) -I$(LIBC)/include -I$(DOOMGENERIC_DIR) -I$(LIBS)/libdesktop/
LDFLAGS := -nostdlib -static -no-pie -T ../../user.ld
CORE := dummy am_map doomdef doomstat dstrings d_event d_items d_iwad d_loop d_main d_mode d_net f_finale f_wipe g_game hu_lib hu_stuff info i_cdmus i_endoom i_joystick i_scale i_sound i_system i_timer memio m_argv m_bbox m_cheat m_config m_controls m_fixed m_menu m_misc m_random p_ceilng p_doors p_enemy p_floor p_inter p_lights p_map p_maputl p_mobj p_plats p_pspr p_saveg p_setup p_sight p_spec p_switch p_telept p_tick p_user r_bsp r_data r_draw r_main r_plane r_segs r_sky r_things sha1 sounds statdump st_lib st_stuff s_sound tables v_video wi_stuff w_checksum w_file w_main w_wad z_zone w_file_stdc i_input i_video doomgeneric
OBJS := $(addprefix build/,$(addsuffix .o,$(CORE))) build/doomgeneric_sulfuros.o
all: check-source doomgeneric.elf
check-source:
	@test -f "$(DOOMGENERIC_DIR)/doomgeneric.c" || { echo "DoomGeneric source missing; run 'make doom-deps' from the repository root"; exit 1; }
build/%.o: $(DOOMGENERIC_DIR)/%.c $(LIBDESKTOP_H) $(LIBC_HEADERS) | build
	$(CC) $(CFLAGS) -c $< -o $@
build/doomgeneric_sulfuros.o: doomgeneric_sulfuros.c $(LIBDESKTOP_H) $(LIBC_HEADERS) | build
	$(CC) $(CFLAGS) -c $< -o $@
build:; mkdir -p build
doomgeneric.elf: $(OBJS) $(LIBC)/build/crt0.o $(LIBC)/build/libc.a $(LIBS)/libdesktop/libdesktop.a
	$(LD) $(LDFLAGS) $(LIBC)/build/crt0.o $(OBJS) $(LIBS)/libdesktop/libdesktop.a $(LIBC)/build/libc.a -o $@
clean:; rm -rf build doomgeneric.elf
.PHONY: all clean check-source
