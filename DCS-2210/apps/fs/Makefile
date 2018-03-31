ROOTDIR = ../..
include $(ROOTDIR)/Rules.make

DIRS = etc script wav $(MODULES)
BRANDS = nb ap tc be vt vd fmm aue ix fxc

ifeq ($(PLATFORM), dm365)
	MODULES = modules.dm365
else
	MODULES = modules
endif

ifeq ($(MODEL), $(filter $(MODEL), 7411 7421 7431 7451))
	FILE = loadmodules_7411.sh
	INIT_FILE = start_apps_7411
	DIRS += www.appro
else ifeq ($(MODEL), 7415)
	FILE = loadmodules_7411.sh
	INIT_FILE = start_apps_7411
	DIRS += www.7315
else ifeq ($(MODEL), 7415B)
	FILE = loadmodules_7411.sh
	INIT_FILE = start_apps_7411
	DIRS += www.7315
else ifeq ($(MODEL), $(filter $(MODEL), 7413 7414))
	FILE = loadmodules_7411.sh
	INIT_FILE = start_apps_7411
	DIRS += www.appro
else ifeq ($(MODEL), $(filter $(MODEL), 7411A 7421A 7431A 7451A))
	FILE = loadmodules_7411.sh
	INIT_FILE = start_apps_7411
	DIRS += www.appro
else ifeq ($(MODEL), $(filter $(MODEL), 7411B 7421B 7431B 7451B))
	FILE = loadmodules_7411.sh
	INIT_FILE = start_apps_7411
	DIRS += www.appro
else ifeq ($(MODEL), $(filter $(MODEL), 7465A 7465B))
	FILE = loadmodules_ov9715_128M.sh
ifeq ($(SUPPORT_WIFI), 1)
	INIT_FILE = start_apps_wifi_128m
else
	INIT_FILE = start_apps_ov9715_128M
endif
	DIRS += www.7315
else ifeq ($(MODEL), $(filter $(MODEL), 7465C 7465D))
	FILE = loadmodules_ov2715.sh
ifeq ($(SUPPORT_WIFI), 1)
	INIT_FILE = start_apps_wifi_256m
else
	INIT_FILE = start_apps_7513
endif
        DIRS += www.7315
else ifeq ($(MODEL), $(filter $(MODEL), 7513 7523 7533 7553 7553B))
	FILE = loadmodules_7513.sh
	INIT_FILE = start_apps_7513
	DIRS += www.appro
else ifeq ($(MODEL), 7515)
	FILE = loadmodules_7411.sh
	INIT_FILE = start_apps_7411
	DIRS += www.7315
else ifeq ($(MODEL), $(filter $(MODEL), 7516T_2A 7526T_2A 7536T_2A 7556T_2A))
	FILE = loadmodules_7411.sh
	INIT_FILE = start_apps_7411
	DIRS += www.appro
else ifeq ($(MODEL), $(filter $(MODEL), 7516T_3A 7526T_3A 7536T_3A 7556T_3A))
	FILE = loadmodules_7411.sh
	INIT_FILE = start_apps_7411
	DIRS += www.appro
else ifeq ($(MODEL), $(filter $(MODEL), 7516A_2A 7526A_2A 7536A_2A 7556A_2A))
	FILE = loadmodules_7411.sh
	INIT_FILE = start_apps_7411
	DIRS += www.appro
else ifeq ($(MODEL), $(filter $(MODEL), 7516A_3A 7526A_3A 7536A_3A 7556A_3A))
	FILE = loadmodules_7411.sh
	INIT_FILE = start_apps_7411
	DIRS += www.appro
else ifeq ($(MODEL), 7435)
	FILE = loadmodules_7435.sh
	INIT_FILE = start_apps_7411
	DIRS += www.7315
else ifeq ($(MODEL), 7417)
        FILE = loadmodules_7335.sh
        INIT_FILE = start_apps_7335
        DIRS += www.appro
else ifeq ($(MODEL), 7437)
        FILE = loadmodules_7335.sh
        INIT_FILE = start_apps_7335
        DIRS += www.appro
else ifeq ($(MODEL), 7315)
	FILE = loadmodules_7315.sh
	INIT_FILE = start_apps_7315
	DIRS += www.7315
else ifeq ($(MODEL), 7311)
	FILE = loadmodules_7315.sh
	INIT_FILE = start_apps_7315
	DIRS += www.appro
else ifeq ($(MODEL), 7331)
	FILE = loadmodules_7315.sh
	INIT_FILE = start_apps_7315
	DIRS += www.appro
else ifeq ($(MODEL), 7317)
	FILE = loadmodules_7317.sh
	INIT_FILE = start_apps_7317
	DIRS += www.appro
else ifeq ($(MODEL), 7317S)
	FILE = loadmodules_7317S.sh
	INIT_FILE = start_apps_7317S
	DIRS += www.appro
else ifeq ($(MODEL), 7335)
	FILE = loadmodules_7335.sh
	INIT_FILE = start_apps_7335
	DIRS += www.7315
else ifeq ($(MODEL), 7337)
	FILE = loadmodules_7317.sh
	INIT_FILE = start_apps_7317
	DIRS += www.appro
else ifeq ($(MODEL), 7337S)
	FILE = loadmodules_7317S.sh
	INIT_FILE = start_apps_7317
	DIRS += www.appro
else ifeq ($(MODEL), 7357)
	FILE = loadmodules_7317.sh
	INIT_FILE = start_apps_7317
	DIRS += www.appro
else ifeq ($(MODEL), 2311B)
	FILE = loadmodules_tvp5150_w_H264.sh
	INIT_FILE = start_apps_tvp5150
	DIRS += www.appro
else ifeq ($(MODEL), 2311T)
	FILE = loadmodules_tvp5150_w_H264.sh
	INIT_FILE = start_apps_tvp5150
	DIRS += www.appro
else ifeq ($(MODEL), 2325B)
	FILE = loadmodules_tvp5150_wo_H264.sh
	INIT_FILE = start_apps_tvp5150
	DIRS += www.7315
else ifeq ($(MODEL), 2325HB)
	FILE = loadmodules_tvp5150_w_H264.sh
	INIT_FILE = start_apps_tvp5150
	DIRS += www.7315
else ifeq ($(MODEL), 2405HBE)
	FILE = loadmodules_7411.sh
	INIT_FILE = start_apps_tvp5150
	DIRS += www.7315
else ifeq ($(MODEL), 2401HBE)
	FILE = loadmodules_7411.sh
	INIT_FILE = start_apps_tvp5150
	DIRS += www.appro
else ifeq ($(MODEL), 2415)
        FILE = loadmodules_7411.sh
        INIT_FILE = start_apps_tvp5150
        DIRS += www.7315
endif


.PHONY: $(DIRS) www.common

all: $(DIRS)


etc:
	@install -d $(EXEC_DIR)/$@
	cp -a $(filter-out $@/CVS,$(wildcard $@/*)) $(EXEC_DIR)/$@
	chmod 0600 $(EXEC_DIR)/$@/ca.pem

$(MODULES):
	@install -d $(EXEC_DIR)
	cp -dp $@/*.ko $(EXEC_DIR)

script:
	@install -d $(EXEC_DIR)
	install $(filter-out $@/CVS,$(wildcard $@/*)) $(EXEC_DIR)
	mv $(EXEC_DIR)/$(FILE) $(EXEC_DIR)/loadmodules.sh
	mv $(EXEC_DIR)/$(INIT_FILE) $(EXEC_DIR)/start_apps

wav:
	@install -d $(EXEC_DIR)/$@
	cp $(filter-out $@/CVS,$(wildcard $@/*)) $(EXEC_DIR)/$@

www.common:
	rm -rf $(WWW_DIR)
	install -d $(WWW_DIR)
	install -d $(WWW_DIR)/$(EPTZ_FILES)

www.appro: www.common
	install -d $(WWW_DIR)/$(BRAND)
	@cp $(filter-out $@/CVS $(addprefix $@/,$(BRANDS) $(EPTZ_FILES)),$(wildcard $@/*)) $(WWW_DIR)
	@cp $(filter-out $@/$(BRAND)/CVS,$(wildcard $@/$(BRAND)/*)) $(WWW_DIR)/$(BRAND)

www: www.common
	@cp $(filter-out $@/CVS,$(wildcard $@/*)) $(WWW_DIR)

www.7315: www.common
	@cp $(filter-out $@/CVS $(addprefix $@/, $(EPTZ_FILES)),$(wildcard $@/*)) $(WWW_DIR)

clean:

cleanall:

install: all


