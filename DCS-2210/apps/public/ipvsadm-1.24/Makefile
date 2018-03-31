#
#      ipvsadm - IP Virtual Server ADMinistration program
#                for IPVS NetFilter Module in kernel 2.4
#
#      Version: $Id: Makefile,v 1.1 2009/01/20 06:13:28 mikewang Exp $
#
#      Authors: Wensong Zhang <wensong@linux-vs.org>
#               Peter Kese <peter.kese@ijs.si>
#
#      This file:
#
#      ChangeLog
#
#      Wensong        :   Modified the Makefile and the spec files so
#                     :   that rpms can be created with ipvsadm alone
#      P.Copeland     :   Modified the Makefile and the spec files so
#                     :   that it is possible to create rpms on the fly
#                     :   using 'make rpms'
#                     :   Also added NAME, VERSION and RELEASE numbers to
#                     :   the Makefile
#      Horms          :   Updated to add config_stream.c dynamic_array.c
#                     :   Added autodetection of libpot
#                     :   Added BUILD_ROOT support
#      Wensong        :   Changed the OBJS according to detection
#      Ratz           :   Fixed to use the correct CFLAGS on sparc64
#

ROOTDIR = ../../..
include $(ROOTDIR)/Rules.make

INSTALLDIR = $(EXEC_DIR)

NAME		= ipvsadm
VERSION		= $(shell cat VERSION)
RELEASE		= 6
SCHEDULERS	= "$(shell cat SCHEDULERS)"
PROGROOT	= $(shell basename `pwd`)
ARCH		= $(shell uname -m)
RPMSOURCEDIR	= $(shell rpm --eval '%_sourcedir')
RPMSPECDIR	= $(shell rpm --eval '%_specdir')

CC		= arm_v5t_le-gcc
INCLUDE		= -I$(LINUXKERNEL_INSTALL_DIR)/include -I.. -I.
SBIN		= $(BUILD_ROOT)/sbin
MANDIR		= usr/man
MAN		= $(BUILD_ROOT)/$(MANDIR)/man8
INIT		= $(BUILD_ROOT)/etc/rc.d/init.d
MKDIR		= mkdir
INSTALL		= install
STATIC_LIBS	= libipvs/libipvs.a

ifeq "${ARCH}" "sparc64"
    CFLAGS = -Wall -Wunused -Wstrict-prototypes -g -O2 -m64 -pipe -mcpu=ultrasparc -mcmodel=medlow
else
    CFLAGS = -Wall -Wunused -Wstrict-prototypes -g -O2
endif


#####################################
# No servicable parts below this line

RPMBUILD = $(shell				\
	if [ -x /usr/bin/rpmbuild ]; then	\
		echo "/usr/bin/rpmbuild";	\
	else					\
		echo "/bin/rpm";		\
	fi )

ifeq (,$(FORCE_GETOPT))
LIB_SEARCH = /lib /usr/lib /usr/local/lib
POPT_LIB = $(shell for i in $(LIB_SEARCH); do \
  if [ -f $$i/libpopt.a ]; then \
    if nm $$i/libpopt.a | fgrep -q poptGetContext; then \
	echo "-L$$i -lpopt"; \
    fi; \
  fi; \
done)
endif

ifneq (,$(POPT_LIB))
POPT_DEFINE = -DHAVE_POPT
endif

OBJS		= ipvsadm.o config_stream.o dynamic_array.o
LIBS		= $(POPT_LIB)
DEFINES		= -DVERSION=\"$(VERSION)\" -DSCHEDULERS=\"$(SCHEDULERS)\" \
		  $(POPT_DEFINE)
DEFINES		+= $(shell if [ ! -f ../ip_vs.h ]; then	\
		     echo "-DHAVE_NET_IP_VS_H"; fi;)


.PHONY	= all clean install dist distclean rpm rpms

all:            libs ipvsadm

libs:
		make -C libipvs

ipvsadm:	$(OBJS) $(STATIC_LIBS)
		$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

install:        all
		cp -a $(NAME) $(INSTALLDIR)
		cp -af ipvsadm.sh $(INSTALLDIR)

clean:
		rm -f ipvsadm $(NAME).spec $(NAME)-$(VERSION).tar.gz
		rm -rf debian/tmp
		find . -name '*.[ao]' -o -name "*~" -o -name "*.orig" \
		  -o -name "*.rej" -o -name core | xargs rm -f
		make -C libipvs clean

distclean:	clean

dist:		distclean
		sed -e "s/@@VERSION@@/$(VERSION)/g" \
		    -e "s/@@RELEASE@@/$(RELEASE)/g" \
		    < ipvsadm.spec.in > ipvsadm.spec
		rm -f $(NAME)-$(VERSION)
		ln -s . $(NAME)-$(VERSION)
		tar czvf $(NAME)-$(VERSION).tar.gz			\
		    --exclude CVS					\
		    --exclude $(NAME)-$(VERSION)/$(NAME)-$(VERSION)	\
		    --exclude $(NAME)-$(VERSION).tar.gz			\
		    $(NAME)-$(VERSION)/*
		rm -f $(NAME)-$(VERSION)

rpms:		dist
		cp $(NAME)-$(VERSION).tar.gz $(RPMSOURCEDIR)/
		cp $(NAME).spec $(RPMSPECDIR)/
		$(RPMBUILD) -ba $(RPMSPECDIR)/$(NAME).spec

srpm:		dist
		cp $(NAME)-$(VERSION).tar.gz $(RPMSOURCEDIR)/
		cp $(NAME).spec $(RPMSPECDIR)/
		$(RPMBUILD) -bs $(RPMSPECDIR)/$(NAME).spec

deb:		debs

debs:
		dpkg-buildpackage

%.o:	%.c
		$(CC) $(CFLAGS) $(INCLUDE) $(DEFINES) -c -o $@ $<
