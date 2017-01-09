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

# make 'release' the default build type
BTYPE ?= release

# if the build directory is not given, make it reflect the board name.
BUILD ?= build/$(BOARD)/$(BTYPE)

include ../py/mkenv.mk

CROSS_COMPILE = xtensa-esp32-elf-

ESP_IDF_PATH = $(IDF_PATH)
ESP_IDF_COMP_PATH = $(ESP_IDF_PATH)/components

CFLAGS_XTENSA = -DESP_PLATFORM -Og -g3 -Wpointer-arith -Werror -Wno-error=unused-function \
                -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wl,-EL \
                -mlongcalls -Wall -ffunction-sections -fdata-sections -fno-common

CFLAGS = $(CFLAGS_XTENSA) -nostdlib -std=gnu99 -g3 -fstrict-volatile-bitfields -Iboards/$(BOARD)

LDFLAGS = -nostdlib -Wl,-Map=$(@:.elf=.map) -Wl,--no-check-sections -u call_user_start_cpu0
LDFLAGS += -Wl,-static -Wl,--undefined=uxTopUsedPriority -Wl,--gc-sections

LIBS = -L$(ESP_IDF_COMP_PATH)/esp32/lib -L$(ESP_IDF_COMP_PATH)/esp32/ld -L$(ESP_IDF_COMP_PATH)/bt/lib \
       -L$(ESP_IDF_COMP_PATH)/esp32 -L$(ESP_IDF_COMP_PATH)/newlib/lib -Lbootloader \
       -Llib -lnvs_flash -ltcpip_adapter -L$(BUILD) -lhal -lcore -lwps -lcoexist \
       -lnet80211 -lphy lib/libpp.a -lrtc -llog -lsmartconfig -lwpa -lbtdm_app \
       -lspi_flash -lnvs_flash -lgcc -lnghttp -ldriver -lesp32 -lexpat \
       $(ESP_IDF_COMP_PATH)/newlib/lib/libm.a $(ESP_IDF_COMP_PATH)/newlib/lib/libc.a \
       -lfreertos -ljson -llwip -lnewlib -lvfs -lopenssl -lmbedtls -lwpa_supplicant \
       -lxtensa-debug-module -lethernet -lbt -lsdmmc

B_LIBS = -Lbootloader/lib -Lbootloader -L$(BUILD)/bootloader \
         -L$(ESP_IDF_COMP_PATH)/esp32/lib -Lbootloader -llog -lcore -lbootloader_support \
         -lspi_flash

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
