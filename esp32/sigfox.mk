ifeq ($(BOARD), $(filter $(BOARD), SIPY))
SFX_OBJ += $(addprefix $(BUILD)/, $(APP_SIGFOX_SRC_SIPY_C:.c=.o) $(APP_SIGFOX_TARGET_SRC_C:.c=.o) $(APP_SIGFOX_SPI_SRC_C:.c=.o))
endif
ifeq ($(BOARD), $(filter $(BOARD), LOPY4 FIPY))
SFX_OBJ += $(addprefix $(BUILD)/, $(APP_SIGFOX_SRC_FIPY_LOPY4_C:.c=.o) $(APP_SIGFOX_SPI_SRC_C:.c=.o))
endif

OBJ += $(SFX_OBJ)

# the build process generates .d files and expects the same directory structure in $(BUILD) as it is in the sources, so we need to mkdir them
SFX_DEP_DIRS := $(BUILD)/sigfox $(BUILD)/sigfox/src $(BUILD)/sigfox/src/targets $(BUILD)/lora
$(SFX_DEP_DIRS):
	$(MKDIR) -p $@

all: $(SFX_DEP_DIRS) $(BUILD)/sigfox/sigfox.a

# $(SFX_OBJ): | CHECK_DEP
# $(SFX_OBJ): | $(GEN_PINS_HDR)
$(SFX_OBJ): | $(HEADER_BUILD)/qstrdefs.generated.h $(HEADER_BUILD)/mpversion.h

$(BUILD)/sigfox/sigfox.a: $(SFX_OBJ) 
	$(ECHO) "AR $@"
	$(Q) rm -f $@
	$(Q) $(AR) cru $@ $^
