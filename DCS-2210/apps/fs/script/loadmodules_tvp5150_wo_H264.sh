#!/bin/sh
insmod cmemk.ko phys_start=0x85D9B000 phys_end=0x88000000 pools=26x4096,3x8192,3x12288,3x16384,3x45056,3x81920,6x208896,3x1531904,1x29618176
./mapdmaq
insmod fm4005.ko
insmod osd.ko
insmod dm350mmap.ko
rm -f /dev/dm350mmap
mknod /dev/dm350mmap c `awk "\\$2==\"dm350mmap\" {print \\$1}" /proc/devices` 0
insmod sbull.ko
insmod musb_hdrc.ko
insmod ioport.ko
udevstart
mkdosfs -v /dev/sbulla
mount /dev/sbulla /mnt/ramdisk
