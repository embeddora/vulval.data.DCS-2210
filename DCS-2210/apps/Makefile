ROOTDIR = ..
include $(ROOTDIR)/Rules.make

SUBDIRS = driver lib fs public

DEBUGSUBDIRS = $(addsuffix .debug, $(SUBDIRS))
RELEASESUBDIRS = $(addsuffix .release, $(SUBDIRS))
#CLEANSUBDIRS = $(addsuffix .clean, $(filter-out lib fs, $(SUBDIRS)))
CLEANSUBDIRS = $(addsuffix .clean, $(SUBDIRS))
CLEANALLDIRS = $(addsuffix .cleanall, $(SUBDIRS))
INSTALLSUBDIRS = $(addsuffix .install, $(SUBDIRS))

.PHONY: $(SUBDIRS) $(DEBUGSUBDIRS) $(RELEASESUBDIRS) $(INSTALLSUBDIRS) \
	$(CLEANSUBDIRS) $(CLEANALLDIRS) private

all:	private $(SUBDIRS)

$(SUBDIRS):
	@echo
	@echo Making all in subdirectory $@...
	@$(MAKE) -C $@

install: private $(INSTALLSUBDIRS)
	@install -d $(EXEC_DIR)
	@install -d $(WWW_DIR)

uninstall:
	rm -rf $(EXEC_DIR)/*
	rm -rf $(WWW_DIR)/*

private:
	@install -d $(EXEC_DIR)
	cp -a $@/* $(EXEC_DIR)

firmware: install $(FIRMWARE_DIR)/host_mkfm $(FIRMWARE_DIR)/firmware.conf
	install -m 0644 $(LINUXKERNEL_INSTALL_DIR)/arch/arm/boot/uImage $(FIRMWARE_DIR)/kernel.img
	/sbin/mkfs.jffs2 -l -e 0x4000 -n -U -d $(EXEC_DIR) -o $(FIRMWARE_DIR)/ipnc.jffs2
	/sbin/mkfs.jffs2 -l -e 0x4000 -n -U -d $(WWW_DIR) -o $(FIRMWARE_DIR)/www.jffs2
	cd $(FIRMWARE_DIR)
	$(FIRMWARE_DIR)/host_mkfm -c $(FIRMWARE_DIR)/firmware.conf -o $(FIRMWARE_DIR)/update.bin.$(BRAND)

convert: $(FIRMWARE_DIR)/host_mkfm $(FIRMWARE_DIR)/firmware.conf
	@mkdir -p $(FIRMWARE_DIR)/convert
	cd $(FIRMWARE_DIR)
	$(FIRMWARE_DIR)/host_mkfm -c $(FIRMWARE_DIR)/firmware.conf -o $(FIRMWARE_DIR)/convert/update.bin.convert -m 0

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

clean: $(CLEANSUBDIRS)

$(CLEANSUBDIRS):
	cd $(basename $@) && $(MAKE) clean

cleanall: $(CLEANALLDIRS)

depend:

$(CLEANALLDIRS):
	cd $(basename $@) && $(MAKE) cleanall
