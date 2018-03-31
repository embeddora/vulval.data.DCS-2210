#
#  File: Makefile
# 
#  Description: Makefile for the osd code
#
############################################################################

ifndef SUBDIRS
	ROOTDIR = ../../..
	include $(ROOTDIR)/Rules.make
endif

PWD = ${shell pwd}
KERNEL_SRC = $(LINUXKERNEL_INSTALL_DIR)
INSTALLDIR = $(EXEC_DIR)

ifndef EXTRA_CFLAGS
	kmods	= osd
	obj-m	:= $(kmods).o
else
	kmods	= osdfm
	obj-m	:= $(kmods).o
	$(kmods)-objs := osd.o
endif

all: $(kmods).ko

install: all
	@install -d $(INSTALLDIR) 
	cp -a $(kmods).ko $(INSTALLDIR)

#$(kmods).ko: $(kmods).c
#	make -C $(KERNEL_SRC) SUBDIRS=$(PWD) modules

#osdfm: $(kmods).c
#	make -C $(KERNEL_SRC) SUBDIRS=$(PWD) EXTRA_CFLAGS=-DKERNEL_FONT modules

clean:
#	rm -f *.o *.ko *.mod.c .*.cmd core
#	rm -rf .tmp_versions
