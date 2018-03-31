#!/bin/sh

#./av_capture_load.sh

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


