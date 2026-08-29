#!/bin/bash
set -e

# made by @offihito

TINYGL_REPO="https://github.com/C-Chads/TinyGL.git"
DEST_DIR="user/libs/tinygl"

echo ":: Porting TinyGL to sulfurOS..."

if [ ! -d "$DEST_DIR" ]; then
    echo ":: Cloning TinyGL into $DEST_DIR..."
    git clone "$TINYGL_REPO" "$DEST_DIR"
else
    echo ":: TinyGL already exists in $DEST_DIR, skipping clone."
fi

cd "$DEST_DIR"

echo ":: Generating Makefile..."
cat << 'EOF' > Makefile
CC   := x86_64-elf-gcc
AR   := x86_64-elf-ar
LIBC := ../../libc

CFLAGS := -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
          -fno-PIE -fno-pic -m64 -march=x86-64 \
          -mno-red-zone -Wall -Wextra -std=gnu11            \
          -I$(LIBC)/include -Iinclude -Isrc -D__sulfur__

SRCS := src/api.c src/arrays.c src/clear.c src/clip.c src/get.c \
        src/init.c src/light.c src/matrix.c src/memory.c src/misc.c \
        src/msghandling.c src/select.c src/specbuf.c src/texture.c \
        src/vertex.c src/zbuffer.c src/zline.c src/zmath.c \
        src/ztriangle.c src/zraster.c src/ztext.c src/list.c \
        src/zpostprocess.c src/image_util.c

OBJS := $(SRCS:.c=.o)

all: libTinyGL.a

libTinyGL.a: $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) libTinyGL.a

.PHONY: all clean
EOF

echo ":: Patching TinyGL..."
grep -l "#include <math.h>" src/*.c src/*.h | xargs sed -i 's/"math.h"/<math.h>/' || true
gsed -i 's/static void gl_free/static inline void gl_free/' include/zbuffer.h
gsed -i 's/static void\* gl_malloc/static inline void\* gl_malloc/' include/zbuffer.h
gsed -i 's/static void\* gl_zalloc/static inline void\* gl_zalloc/' include/zbuffer.h

echo ":: TinyGL porting setup complete."
echo ":: To build libc and TinyGL, run: "
echo "   make -C user/libc clean && make -C user/libc"
echo "   make -C user/libs/tinygl"