CC := x86_64-elf-gcc
LD := x86_64-elf-ld
LIBC := ../../libc
LIBS := ../../libs

CFLAGS := -ffreestanding -nostdlib -fno-builtin -fno-stack-protector    \
          -fno-PIE -fno-pic -m64 -march=x86-64                          \
          -mno-red-zone -Wall -Wextra -std=gnu11                        \
          -I$(LIBC)/include                                             \
          -I$(LIBS)/ui16                                                \
          -I$(LIBS)/tinygl/include                                      \
          -I$(LIBS)/tinygl/src                                          \
          -I$(LIBS)/libdesktop

LDFLAGS := -nostdlib -static -no-pie -T ../../user.ld

OUT := gears.elf

all: clean $(OUT)

$(OUT): gears.o $(LIBC)/build/crt0.o $(LIBC)/build/libc.a $(LIBS)/libdesktop/libdesktop.a $(LIBS)/ui16/ui16.a $(LIBS)/tinygl/libTinyGL.a
	$(LD) $(LDFLAGS) $(LIBC)/build/crt0.o gears.o $(LIBS)/ui16/ui16.a $(LIBS)/libdesktop/libdesktop.a $(LIBS)/tinygl/libTinyGL.a $(LIBC)/build/libc.a -o $@

gears.o: gears.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBC)/build/crt0.o $(LIBC)/build/libc.a:
	$(MAKE) -C $(LIBC)

clean:
	rm -f *.o $(OUT)

.PHONY: all clean