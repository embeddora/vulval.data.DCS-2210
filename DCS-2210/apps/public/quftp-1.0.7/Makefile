ROOTDIR = ../../..
include $(ROOTDIR)/Rules.make

INSTALLDIR = $(EXEC_DIR)

prefix=/usr/local
exec_prefix=${prefix}
MANDIR=${prefix}/man
BINDIR=${exec_prefix}/bin
SRCDIR=.
CC = $(MVTOOL_PREFIX)gcc
STRIP = $(MVTOOL_PREFIX)strip

INSTALL=install
INSTALL_PROGRAM=${INSTALL}
INSTALL_DATA=${INSTALL}
BIN  = quftp upload quftprc

OBJS=ftp.o ftp_data.o ftp_directory.o ftp_login.o ftp_transfer.o ftp_list.o \
	text.o rc.o quftp.o log.o
LIBS += -lreadline -lncurses $(LIB_DIR)/Appro_interface.a  $(LIB_DIR)/sysinfo.a $(LIB_DIR)/sysctrl.a $(LIB_DIR)/file_msg_drv.a

ifeq ($(PLATFORM), dm365)
	LIBS += $(CMEM_INSTALL_DIR)/packages/ti/sdo/linuxutils/cmem/lib/cmem.a470MV
else ifeq ($(PLATFORM), dm355)
	LIBS += $(LIB_DIR)/cmem.a
endif

HEADERS=*.h
DEFS = -DHAVE_LIBNCURSES=1 -DHAVE_LIBREADLINE=1 -DHAVE_DIRENT_H=1 -DSTDC_HEADERS=1 -DHAVE_SYS_WAIT_H=1 -DHAVE_FCNTL_H=1 -DHAVE_MALLOC_H=1 -DHAVE_SYS_TIME_H=1 -DHAVE_UNISTD_H=1 -DTIME_WITH_SYS_TIME=1 -DHAVE_FNMATCH=1 -DHAVE_GETCWD=1 -DHAVE_GETTIMEOFDAY=1 -DHAVE_MKDIR=1 -DHAVE_SOCKET=1 -DHAVE_STRDUP=1 -DHAVE_STRSTR=1 
CFLAGS += -O2 -Wall -pipe -I$(PUBLIC_INCLUDE_DIR) -I$(CMEM_INSTALL_DIR)/packages/ti/sdo/linuxutils/cmem/include

#ifdef LOG_MODE
#        CFLAGS += -DDBGLOG
#endif

ifdef DEBUG_MODE
	CFLAGS += -g -DDEBUG
	LDFLAGS += -g
endif

.SUFFIXES:
.SUFFIXES: .c .o

.PHONY:	install clean

all:	quftp

install: all
	cp $(BIN) $(INSTALLDIR)

install-strip:
	$(MAKE) INSTALL_PROGRAM='$(INSTALL_PROGRAM) -s' install

uninstall:
	rm -f ${DESTDIR}${BINDIR}/quftp
	rm -f ${DESTDIR}${MANDIR}/man1/quftp.1

clean:
	rm -f *.o quftp

distclean:
	rm -f config.status config.log config.cache Makefile

quftp:	quftp.c ${OBJS} ${HEADERS}
	${CC} -I. -o $@ ${OBJS} ${CFLAGS} ${LIBS}

temp:	temp.o

ftp.o:		${HEADERS}
ftp_data.o:	${HEADERS}
ftp_directory.o:${HEADERS}
ftp_login.o:	${HEADERS}
ftp_transfer.o:	${HEADERS}
ftp_list.o:	${HEADERS}
text.o:		${HEADERS}
rc.o:		${HEADERS}
quftp.o:	${HEADERS}

