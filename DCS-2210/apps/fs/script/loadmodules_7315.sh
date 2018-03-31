#!/bin/sh
insmod cmemk.ko phys_start=0x84600000 phys_end=0x88000000 pools=1x4096,1x2408,1x100,1x2120,5x1228800,20x1024,1x3072,1x1656,2x41984,1x142848,11x38400,3x691200,1x11328,3x81920,3x4153344,1x38000000

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
