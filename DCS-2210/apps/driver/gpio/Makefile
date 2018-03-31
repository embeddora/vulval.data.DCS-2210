#
#  File: Makefile
# 
#  Description: Makefile for the gpio code
#
############################################################################

ifndef SUBDIRS
	ROOTDIR = ../../..
	include $(ROOTDIR)/Rules.make
endif

PWD = ${shell pwd}
KERNEL_SRC = $(LINUXKERNEL_INSTALL_DIR)
INSTALLDIR = $(EXEC_DIR)

kmods	= gpio
obj-m	:= -DEXPORT_SYMTAB
obj-m	:= $(kmods).o

all: $(kmods).ko

install: all
	@install -d $(INSTALLDIR)
	cp -a $(kmods).ko $(INSTALLDIR)

#$(kmods).ko: $(kmods).c
#	make -C $(KERNEL_SRC) SUBDIRS=$(PWD) modules

clean:
#	rm -f *.o *.ko $(kmods).mod.c .*.cmd core
#	rm -rf .tmp_versions
