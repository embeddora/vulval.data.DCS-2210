ROOTDIR = ../..
include $(ROOTDIR)/Rules.make

SUBDIRS = boa-0.94.13  quftp-1.0.7 ez-ipupdate-3.0.10 esmtp-0.6.0 wondershaper-1.1a pppoe ushare-1.1a miniupnpc dosfstools-3.0.9

ifeq ($(BRAND), dlink)
	SUBDIRS += ipvsadm-1.24
endif

ifeq ($(SUPPORT_WIFI), 1)
	SUBDIRS += wpa_supplicant
endif

DEBUGSUBDIRS = $(addsuffix .debug, $(SUBDIRS))
RELEASESUBDIRS = $(addsuffix .release, $(SUBDIRS))
CLEANSUBDIRS = $(addsuffix .clean, $(filter-out avahi-0.6.23, $(SUBDIRS)))
CLEANALLDIRS = $(addsuffix .cleanall, $(SUBDIRS))
INSTALLSUBDIRS = $(addsuffix .install, $(SUBDIRS))

.PHONY: $(SUBDIRS) $(DEBUGSUBDIRS) $(RELEASESUBDIRS) $(INSTALLSUBDIRS) \
	$(CLEANSUBDIRS)

all:	$(SUBDIRS)

$(SUBDIRS):
	@echo
	@echo Making all in subdirectory $@...
	@$(MAKE) -C $@

install: $(INSTALLSUBDIRS)

$(DEBUGSUBDIRS):
	@echo
	@echo Executing make debug in subdirectory $(basename $@)...
	@cd $(basename $@) && $(MAKE) debug

debug:	$(DEBUGSUBDIRS)

$(RELEASESUBDIRS):
	@echo
	@echo Executing make release in subdirectory $(basename $@)...
	@cd $(basename $@) && $(MAKE) release

release:	$(RELEASESUBDIRS)

$(INSTALLSUBDIRS):
	@echo
	@echo Executing make install in subdirectory $(basename $@)...
	@cd $(basename $@) && $(MAKE) install

clean:	$(CLEANSUBDIRS)

$(CLEANSUBDIRS):
	@cd $(basename $@) && $(MAKE) clean

cleanall: $(CLEANALLDIRS)

$(CLEANALLDIRS):
	cd $(basename $@) && $(MAKE) clean
