#!/bin/sh

./av_capture_unload.sh 2>/dev/null

./csl_load.sh
./drv_load.sh

#insmod cmemk.ko phys_start=0x85000000 phys_end=0x88000000 pools=50x512,2x4096,2x8192,2x16384,1x32768,1x51200,1x102400,1x4096000
insmod cmemk.ko phys_start=0x86C00000 phys_end=0x90000000 allowOverlap=1 phys_start_1=0x00001000 phys_end_1=0x00008000 pools_1=1x28672
#insmod cmemk.ko phys_start=0x83C00000 phys_end=0x88000000 

insmod edmak.ko
insmod irqk.ko
insmod dm365mmap.ko
