# DoomGeneric for sulfurOS

From the repository root, fetch DoomGeneric and add your legally obtained WAD:

    make doom-deps
    cp /path/to/doom1.wad ./doom1.wad
    make

The normal OS build copies both files into the initrd and starts Doom as init.
The backend uses `/dev/fb0`, stdin keyboard events,
and the monotonic clock syscall. Audio is currently disabled.

The default display scale is 2x. On a slow emulator, build with
`make DOOM_SCALE=1` to minimize framebuffer traffic.
