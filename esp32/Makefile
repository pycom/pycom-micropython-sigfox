#
# Copyright © 2016, Pycom Limited.
#
# This software is licensed under the GNU GPL version 3 or any
# later version, with permitted additional terms. For more information
# see the Pycom Licence v1.0 document supplied with this file, or
# available at https://www.pycom.io/opensource/licensing
#

# select the board to build for: if not given on the command line
# default to WIPY
BOARD ?= WIPY
ifeq ($(wildcard boards/$(BOARD)/.),)
    $(error Invalid BOARD specified)
endif

TARGET ?= app

OEM ?= 0

# make 'release' the default build type
BTYPE ?= release

LORA_BAND ?= USE_BAND_868
ifeq ($(BOARD), $(filter $(BOARD), LOPY FIPY))
    ifeq ($(LORA_BAND), USE_BAND_868)
        LORA_FREQ = 868
    else
        LORA_FREQ = 915
    endif
    BUILD = build/$(BOARD)_$(LORA_FREQ)/$(BTYPE)
else
    LORA_FREQ =
    BUILD = build/$(BOARD)/$(BTYPE)
endif

FROZEN_MPY_DIR = frozen

include ../py/mkenv.mk

CROSS_COMPILE = xtensa-esp32-elf-

ESP_IDF_PATH = $(IDF_PATH)
ESP_IDF_COMP_PATH = $(ESP_IDF_PATH)/components

CFLAGS_XTENSA = -DESP_PLATFORM -Wpointer-arith -Wall -Werror=all -Wno-error=unused-function \
                -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wl,-EL \
                -mlongcalls -Wall -ffunction-sections -fdata-sections -fno-common \
                -Wno-sign-compare -Wno-old-style-declaration -fstrict-volatile-bitfields \
                -Wno-error=deprecated-declarations -Wno-unused-parameter -DWITH_POSIX \

CFLAGS_XTENSA_OPT = -Os
CFLAGS_XTENSA_PSRAM = -mfix-esp32-psram-cache-issue

CFLAGS = $(CFLAGS_XTENSA) $(CFLAGS_XTENSA_PSRAM) $(CFLAGS_XTENSA_OPT) -nostdlib -std=gnu99 -g3 -ggdb -fstrict-volatile-bitfields -Iboards/$(BOARD)
CFLAGS_SIGFOX = $(CFLAGS_XTENSA) -O2 -nostdlib -std=gnu99 -g3 -ggdb -fstrict-volatile-bitfields -Iboards/$(BOARD)

LDFLAGS = -nostdlib -Wl,-Map=$(@:.elf=.map) -Wl,--no-check-sections -u call_user_start_cpu0
LDFLAGS += -Wl,-static -Wl,--undefined=uxTopUsedPriority -Wl,--gc-sections

LIBS = -L$(ESP_IDF_COMP_PATH)/esp32/lib -L$(ESP_IDF_COMP_PATH)/esp32/ld -L$(ESP_IDF_COMP_PATH)/bt/lib \
       -L$(ESP_IDF_COMP_PATH)/esp32 -L$(ESP_IDF_COMP_PATH)/newlib/lib -Lbootloader \
       -Llib -lnvs_flash -ltcpip_adapter -L$(BUILD) -lhal -lcore -lwps -lcoexist \
       -lnet80211 -lphy -lpp -lrtc -llog -lsmartconfig -lwpa -lwpa2 -lbtdm_app -lcxx -u __cxa_guard_dummy \
       -lsigfox -lspi_flash -lnvs_flash -lgcc -lnghttp -ldriver -lesp32 -lexpat -lpthread -lesp_adc_cal \
       $(ESP_IDF_COMP_PATH)/esp32/libstdc++-psram-workaround.a \
       $(ESP_IDF_COMP_PATH)/newlib/lib/libm-psram-workaround.a \
       $(ESP_IDF_COMP_PATH)/newlib/lib/libc-psram-workaround.a \
       -lfreertos -ljson -ljsmn -llwip -lnewlib -lvfs -lopenssl -lmbedtls -lwpa_supplicant \
       -lxtensa-debug-module -lbt -lsdmmc -lsoc -lheap -u ld_include_panic_highint_hdl \

B_LIBS = -Lbootloader/lib -Lbootloader -L$(BUILD)/bootloader \
         -L$(ESP_IDF_COMP_PATH)/esp32/lib -llog -lcore -lbootloader_support \
         -lspi_flash -lsoc -lmicro-ecc -lgcc -lstdc++ -lgcov

# qstr definitions (must come before including py.mk)
QSTR_DEFS = qstrdefsport.h $(BUILD)/pins_qstr.h
# include py core make definitions
include ../py/py.mk
include application.mk

include ../py/mkrules.mk

# convenience variable for printing an 80 asterisk wide separator line
SEPARATOR:="*******************************************************************************"

# $(sort $(var)) removes duplicates
#
# The net effect of this, is it causes the objects to depend on the
# object directories (but only for existence), and the object directories
# will be created if they don't exist.
BOOT_OBJ_DIRS = $(sort $(dir $(BOOT_OBJ)))
$(BOOT_OBJ): | $(BOOT_OBJ_DIRS)
$(BOOT_OBJ_DIRS):
	$(MKDIR) -p $@
