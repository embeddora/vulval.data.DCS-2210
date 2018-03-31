#!/bin/sh

#./av_capture_load.sh

./av_capture_unload.sh 2>/dev/null

./csl_load.sh
./drv_load.sh

insmod cmemk.ko phys_start=0x83E00000 phys_end=0x88000000 allowOverlap=1 phys_start_1=0x00001000 phys_end_1=0x00008000 pools_1=1x28672
insmod edmak.ko
insmod irqk.ko
insmod dm365mmap.ko
insmod osd.ko
insmod gpio.ko
umount /mnt/ramdisk 2>/dev/null
rmmod sbull.ko 2>/dev/null 
insmod sbull.ko 
insmod musb_hdrc.ko
insmod ioport.ko
udevstart
mkdosfs -v /dev/sbulla
mount -t vfat /dev/sbulla /mnt/ramdisk


