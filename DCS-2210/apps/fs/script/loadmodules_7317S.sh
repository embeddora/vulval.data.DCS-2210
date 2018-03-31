#!/bin/sh
#insmod cmemk.ko phys_start=0x85E00000 phys_end=0x88000000 pools=14x345600,3x81920,5x1024,12x10240,3x1173504,1x26600000
#insmod cmemk.ko phys_start=0x85B50000 phys_end=0x88000000 pools=14x345600,3x81920,10x1024,12x10240,3x1173504,1x29618176
 insmod cmemk.ko phys_start=0x85A00000 phys_end=0x88000000 pools=14x345600,3x81920,14x1024,12x10240,3x1173504,1x29618176,3x153600

./mapdmaq

insmod fm4005.ko
insmod osd.ko
insmod dm350mmap.ko
rm -f /dev/dm350mmap
mknod /dev/dm350mmap c `awk "\\$2==\"dm350mmap\" {print \\$1}" /proc/devices` 0
insmod sbull.ko
insmod musb_hdrc.ko
insmod ioport.ko
#insmod mg1264.ko
udevstart
mkdosfs -v /dev/sbulla
mount /dev/sbulla /mnt/ramdisk
