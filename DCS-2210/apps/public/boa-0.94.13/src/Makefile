# Generated automatically from Makefile.in by configure.
# $Id: Makefile.in,v 1.59 2002/03/24 22:20:19 jnelson Exp $

.SUFFIXES:
.SUFFIXES: .o .c
.PHONY: clean mrclean distclean depend all dist

ROOTDIR = ../../../..
include $(ROOTDIR)/Rules.make

# Change these if necessary

YACC = bison -y 
LEX = flex 
CC = $(MVTOOL_PREFIX)gcc 
CPP = $(MVTOOL_PREFIX)gcc -E
CPLUSPLUS = $(MVTOOL_PREFIX)g++
STRIP = $(MVTOOL_PREFIX)strip

INSTALLDIR = $(EXEC_DIR)

LIVE_DIR = $(IPNC_DIR)/lib/$(LIVE_NAME)

srcdir = .
VPATH = .:./../extras
LIBS = $(LIB_DIR)/msg_util.a $(LIB_DIR)/Appro_interface.a \
	$(LIB_DIR)/file_msg_drv.a $(LIB_DIR)/sysctrl.a $(LIB_DIR)/sys_msg_drv.a \
	$(LIB_DIR)/net_util.a $(LIB_DIR)/sys_log.a $(LIB_DIR)/misc_util.a \
	$(LIB_DIR)/ioport.a $(LIB_DIR)/sysinfo.a $(LIB_DIR)/bios_data.a \
	$(LIB_DIR)/gio_lib.a $(LIB_DIR)/osd_lib.a $(LIB_DIR)/syslogapi.a \
	$(LIB_DIR)/libvisca.a \
	$(LIB_DIR)/libliveMedia.a $(LIB_DIR)/libgroupsock.a \
	$(LIB_DIR)/libUsageEnvironment.a $(LIB_DIR)/libBasicUsageEnvironment.a

ifeq ($(PLATFORM), dm365)
	LIBS += $(CMEM_INSTALL_DIR)/packages/ti/sdo/linuxutils/cmem/lib/cmem.a470MV
else ifeq ($(PLATFORM), dm355)
	LIBS += $(LIB_DIR)/sensor_lib.a $(LIB_DIR)/cmem.a
endif

CFLAGS += -Wall -O3 -pipe -fomit-frame-pointer -I$(PUBLIC_INCLUDE_DIR) -I$(CMEM_INSTALL_DIR)/packages/ti/sdo/linuxutils/cmem/include -I$(LINUXKERNEL_INSTALL_DIR)/include
CFLAGS += -I$(LIVE_DIR)/BasicUsageEnvironment/include -I$(LIVE_DIR)/UsageEnvironment/include -I$(LIVE_DIR)/groupsock/include -I$(LIVE_DIR)/liveMedia/include
CFLAGS += -DSERVER_SSL -D_LINUX -D$(shell echo $(LIVE_NAME) | tr [a-z] [A-Z])
LIBS += -lssl -lcrypto -pthread

ifdef LOG_MODE
        CFLAGS += -DDBGLOG
endif

ifdef DEBUG_MODE
	CFLAGS += -DDEBUG
endif
	
CSRCS = alias.c boa.c buffer.c cgi.c cgi_header.c config.c escape.c \
	get.c hash.c ip.c log.c mmap_cache.c pipe.c queue.c read.c \
	request.c response.c select.c signals.c util.c sublog.c appro_api.c \
	web_translate.c file_list.c auth.c rs485.c verify.c rtsp_o_http.c

CPPSRCS = rtsp_over_http.cpp Err.cpp ApproInput.cpp ApproServerMediaSubsession.cpp \
	ApproJPEGStreamSource.cpp ApproJPEGVideoServerMediaSubsession.cpp \
	ApproMPEG4VideoServerMediaSubsession.cpp \
	ApproH264VideoServerMediaSubsession.cpp \
	ApproPCMAudioServerMediaSubsession.cpp \
	ApproG726AudioServerMediaSubsession.cpp \
	RTSP_O_HTTPServer.cpp

OBJS = y.tab.o lex.yy.o $(CSRCS:.c=.o) $(CPPSRCS:.cpp=.o) timestamp.o


all:	boa boa_indexer

install: boa
	install boa $(INSTALLDIR)
	$(STRIP) $(INSTALLDIR)/boa

nfs: boa
	@install -d $(ROOT_FILE_SYS)/opt/$(MODEL)/ipnc
	install boa $(ROOT_FILE_SYS)/opt/$(MODEL)/ipnc

boa:	$(OBJS)
	@echo aaaaa..
	$(CPLUSPLUS) $(CFLAGS) -o $@ $^ $(LIBS)
#	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
	
boa_indexer:	index_dir.o escape.o   
	@echo bbbbb..
# 	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJS) boa core lex.yy.c y.tab.c y.tab.h *~ boa_indexer index_dir.o

distclean:	mrclean

mrclean:	clean
	rm -f config.status config.cache config.h Makefile config.log

# .c.o:
%o: %c
	$(CC) -c $(CFLAGS) $< -o $@

# .cpp.o:
%o: %cpp
	$(CPLUSPLUS) -c $(CFLAGS) $< -o $@

# parser dependencies

y.tab.c y.tab.h:	boa_grammar.y
	$(YACC) -d $^

lex.yy.c:	boa_lexer.l
	$(LEX) $^

# timestamp

timestamp.o:	$(CSRCS) $(CPPSRCS) boa_grammar.y boa_lexer.l

# depend stuff
.depend:
	$(CPP) -MM $(CSRCS) $(CPPSRCS) > .depend
        
depend:
	-rm -f .depend
	$(MAKE) .depend
        
include .depend

# tags
tags:	$(CSRCS) $(CPPSRCS)
	ctags -o tags $^ *.h

# dist
dist:
	$(MAKE) clean
	./makedist.sh
        
# object dump
boa.objdump:    boa
	objdump --disassemble-all --source boa > $@

rtsp_over_http.cpp:				Err.hh

Err.cpp:					Err.hh

ApproInput.cpp:					ApproInput.hh Err.hh

ApproServerMediaSubsession.cpp:			ApproServerMediaSubsession.hh

ApproServerMediaSubsession.hh:			ApproInput.hh
ApproMPEG4VideoServerMediaSubsession.hh:	ApproServerMediaSubsession.hh

ApproMPEG4VideoServerMediaSubsession.cpp:	ApproMPEG4VideoServerMediaSubsession.hh
ApproH264VideoServerMediaSubsession.cpp:	ApproH264VideoServerMediaSubsession.hh

ApproJPEGStreamSource.cpp:			ApproJPEGStreamSource.hh
ApproPCMAudioServerMediaSubsession.cpp:		ApproPCMAudioServerMediaSubsession.hh
ApproG726AudioServerMediaSubsession.cpp:	ApproG726AudioServerMediaSubsession.hh
RTSP_O_HTTPServer.cpp:				RTSP_O_HTTPServer.hh