 # Makefile.  Generated from Makefile.in by configure.
#***********************************************************************
#
# Makefile
#
# Makefile for Roaring Penguin's Linux user-space PPPoE client.
#
# Copyright (C) 2000 Roaring Penguin Software Inc.
#
# This program may be distributed according to the terms of the GNU
# General Public License, version 2 or (at your option) any later version.
#
# LIC: GPL
#
# $Id: Makefile,v 1.3 2009/04/20 14:52:46 AlexLee Exp $
#***********************************************************************

# Version is set ONLY IN THE MAKEFILE!  Don't delete this!
VERSION=3.10

DEFINES=
prefix=/usr
exec_prefix=${prefix}
mandir=${prefix}/share/man
docdir=/usr/share/doc/rp-pppoe-$(VERSION)
install=/usr/bin/install -c
install_dir=/usr/bin/install -c -d
sbindir=${exec_prefix}/sbin
ROOTDIR = ../../..
include $(ROOTDIR)/Rules.make
INSTALLDIR = $(EXEC_DIR)

# Plugin for pppd on Linux
LINUX_KERNELMODE_PLUGIN=
PPPD_INCDIR=/usr/include

# Licensed version
LIC_INCDIR=
LIC_LIBDIR=
LIC_LIB=
LIC_DEFINE=

# PPPoE relay -- currently only supported on Linux
PPPOE_RELAY=pppoe-relay

# Program paths
PPPOE_PATH=$(sbindir)/pppoe
PPPD_PATH=/usr/sbin/pppd

# Include ServPoET version if we're building for ServPoET


# Kernel-mode plugin gets installed here.
PLUGIN_DIR=/etc/ppp/plugins
PLUGIN_PATH=$(PLUGIN_DIR)/rp-pppoe.so

# Configuration file paths
PPPOESERVER_PPPD_OPTIONS=/etc/ppp/pppoe-server-options

PATHS='-DPPPOE_PATH="$(PPPOE_PATH)"' '-DPPPD_PATH="$(PPPD_PATH)"' \
	'-DPLUGIN_PATH="$(PLUGIN_PATH)"' \
	'-DPPPOE_SERVER_OPTIONS="$(PPPOESERVER_PPPD_OPTIONS)"'

LDFLAGS="-static"
CFLAGS += $(LDFLAGS) -g -O2 -Wall -Wstrict-prototypes $(LIC_INCDIR) $(DEFINES) $(LIC_DEFINE) $(PATHS) -Ilibevent
TARGETS=pppoe 
#pppoe-server pppoe-sniff pppoe-relay
PPPOE_SERVER_LIBS=$(LIC_LIBDIR) $(LIC_LIB)

all: $(TARGETS)
	@echo ""
	@echo "Type 'make install' as root to install the software."

pppoe-sniff: pppoe-sniff.o if.o common.o debug.o
	arm_v5t_le-gcc -o pppoe-sniff pppoe-sniff.o if.o common.o debug.o

pppoe-server: pppoe-server.o if.o debug.o common.o md5.o libevent/libevent.a 
	arm_v5t_le-gcc -o pppoe-server  pppoe-server.o if.o debug.o common.o md5.o $(PPPOE_SERVER_LIBS) -Llibevent -levent

pppoe: pppoe.o if.o debug.o common.o ppp.o discovery.o
	#arm_v5t_le-gcc -o pppoe pppoe.o if.o debug.o common.o ppp.o discovery.o
	arm_v5t_le-gcc -o $@ $^  $(LIB_DIR)/file_msg_drv.a  $(LIB_DIR)/sem_util.a

pppoe-relay: relay.o if.o debug.o common.o
	arm_v5t_le-gcc -o pppoe-relay relay.o if.o debug.o common.o

pppoe.o: pppoe.c pppoe.h
	arm_v5t_le-gcc $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o pppoe.o pppoe.c
	#arm_v5t_le-gcc -I /home/kevinlee/workdir/ipnc_app/include -D_XOPEN_SOURCE -c -o discovery.o discovery.c /home/kevinlee/workdir/ipnc_app/lib/file_msg_drv.a /home/kevinlee/workdir/ipnc_app/lib/sem_util.a

discovery.o: discovery.c pppoe.h
	#arm_v5t_le-gcc $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o discovery.o discovery.c
	arm_v5t_le-gcc $(CFLAGS) -I$(IPNC_DIR)/include  -D_XOPEN_SOURCE -c -o discovery.o discovery.c 

ppp.o: ppp.c pppoe.h
	arm_v5t_le-gcc $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o ppp.o ppp.c

md5.o: md5.c md5.h
	arm_v5t_le-gcc $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o md5.o md5.c

pppoe-server.o: pppoe-server.c pppoe.h 
	arm_v5t_le-gcc $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o pppoe-server.o pppoe-server.c

pppoe-sniff.o: pppoe-sniff.c pppoe.h
	arm_v5t_le-gcc $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o pppoe-sniff.o pppoe-sniff.c

if.o: if.c pppoe.h
	arm_v5t_le-gcc $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o if.o if.c

libevent/libevent.a:
	cd libevent && $(MAKE) DEFINES="$(DEFINES)"

common.o: common.c pppoe.h
	arm_v5t_le-gcc $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o common.o common.c

debug.o: debug.c pppoe.h
	arm_v5t_le-gcc $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o debug.o debug.c

relay.o: relay.c relay.h pppoe.h
	arm_v5t_le-gcc $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o relay.o relay.c

# Linux-specific plugin
rp-pppoe.so: plugin/libplugin.a plugin/plugin.o
	arm_v5t_le-gcc -o rp-pppoe.so -shared plugin/plugin.o plugin/libplugin.a

plugin/plugin.o: plugin.c
	arm_v5t_le-gcc '-DRP_VERSION="$(VERSION)"' $(CFLAGS) -I$(PPPD_INCDIR) -c -o plugin/plugin.o -fPIC plugin.c

plugin/libplugin.a: plugin/discovery.o plugin/if.o plugin/common.o plugin/debug.o
	ar -rc $@ $^

plugin/discovery.o: discovery.c
	arm_v5t_le-gcc $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o plugin/discovery.o -fPIC discovery.c

plugin/if.o: if.c
	arm_v5t_le-gcc $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o plugin/if.o -fPIC if.c

plugin/debug.o: debug.c
	arm_v5t_le-gcc $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o plugin/debug.o -fPIC debug.c

plugin/common.o: common.c
	arm_v5t_le-gcc $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o plugin/common.o -fPIC common.c

install: all
	cp  pppoe $(INSTALLDIR)



clean:
	rm -f *.o pppoe pppoe-sniff pppoe-server core rp-pppoe.so plugin/*.o plugin/libplugin.a *~
	test -f licensed-only/Makefile && $(MAKE) -C licensed-only clean || true
	test -f libevent/Makefile && $(MAKE) -C libevent clean || true
	test -f l2tp/Makefile && $(MAKE) -C l2tp clean || true

distclean: clean
	rm -f Makefile config.h config.cache config.log config.status
	rm -f libevent/Makefile
	rm -f ../scripts/pppoe-connect ../scripts/pppoe-start ../scripts/pppoe-stop ../scripts/pppoe-init ../scripts/pppoe-setup ../scripts/pppoe-init-suse ../scripts/pppoe-init-turbolinux

update-version:
	sed -e 's/^Version: .*$$/Version: $(VERSION)/' ../rp-pppoe.spec > ../rp-pppoe.spec.new && mv ../rp-pppoe.spec.new ../rp-pppoe.spec
	sed -e 's+^Source: .*$$+Source: http://www.roaringpenguin.com/pppoe/rp-pppoe-$(VERSION).tar.gz+' ../rp-pppoe.spec > ../rp-pppoe.spec.new && mv ../rp-pppoe.spec.new ../rp-pppoe.spec
	test -f ../servpoet.spec && sed -e 's/^Version: .*$$/Version: $(VERSION)/' ../servpoet.spec > ../servpoet.spec.new && mv ../servpoet.spec.new ../servpoet.spec || true
	test -f ../servpoet.spec && sed -e 's+^Source: .*$$+Source: http://www.roaringpenguin.com/pppoe/servpoet-$(VERSION).tar.gz+' ../servpoet.spec > ../servpoet.spec.new && mv ../servpoet.spec.new ../servpoet.spec || true

# Convenience target for David!  Don't try to use this one.
km:
	./configure --enable-plugin=/home/dfs/Archive/PPP/ppp-2.4.0.pppoe4-patched-dfs

licensed-only:
	$(MAKE) -C licensed-only all VERSION=$(VERSION) SERVPOET_VERSION=$(SERVPOET_VERSION)

l2tp: libevent/libevent.a
	$(MAKE) -C l2tp all

.PHONY: update-version

.PHONY: clean

.PHONY: distclean

.PHONY: rpms

.PHONY: licensed-only

.PHONY: distro

.PHONY: l2tp
