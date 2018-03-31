# Makefile for libipvs
ROOTDIR = ../../../..
include $(ROOTDIR)/Rules.make

CC		= arm_v5t_le-gcc
INCLUDE		= -I$(LINUXKERNEL_INSTALL_DIR)/include
CFLAGS		= -Wall -Wunused -Wstrict-prototypes -g -O2

INCLUDE		+= $(shell if [ -f ../../ip_vs.h ]; then	\
		     echo "-I../../."; fi;)
DEFINES		= $(shell if [ ! -f ../../ip_vs.h ]; then	\
		    echo "-DHAVE_NET_IP_VS_H"; fi;)

.PHONY		= all clean install dist distclean rpm rpms
STATIC_LIB	= libipvs.a

all:		$(STATIC_LIB)

$(STATIC_LIB):	libipvs.o
		ar rv $@ $^

%.o:		%.c
		$(CC) $(CFLAGS) $(INCLUDE) $(DEFINES) -c -o $@ $<

clean:
		rm -f *.[ao] *~ *.orig *.rej core

distclean:	clean
