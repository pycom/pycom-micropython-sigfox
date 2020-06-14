#
# Copyright © 2020, Pycom Limited.
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

IDF_HASH=058a3d77c7dabb1f1c50fef03a8e4bb69e79fd8d
XTENSA_VERSION = "2020r1"

TARGET ?= boot_app

OEM ?= 0

# make 'release' the default build type
BTYPE ?= release

# make Non-Pybytes firmware default
VARIANT ?=BASE

ifeq ($(VARIANT),BASE)
	BUILD_DIR ?=build
else
	BUILD_DIR ?=build-$(VARIANT)
endif

BUILD = $(BUILD_DIR)/$(BOARD)/$(BTYPE)

RELEASE_DIR ?= $(BUILD_DIR)

COPY_IDF_LIB ?= 0

LTE_LOG_BUFF ?= 0
RGB_LED ?= enable

# by default openthread over LoRa is enabled
OPENTHREAD = off

# by default Secure Boot and Flash Encryption are disabled
SECURE ?= off

ifeq ($(SECURE), on)
	# filename of the private key, used for 2 things:
	# 1. signing partitions.bin and board.bin
	# 2. extracting public key, which is embedded into bootloader.bin
	# Generation is done using command:
	# espsecure.py generate_signing_key secure_boot_signing_key.pem
	SECURE_KEY ?= secure_boot_signing_key.pem

    # Flash Encryption key is generated using command:
	# espsecure.py generate_flash_encryption_key flash_encryption_key.bin
	# Writing it is done using:
	# espefuse.py --port PORT burn_key flash_encryption flash_encryption_key.bin
    	ENCRYPT_KEY ?= flash_encryption_key.bin
else
$(info Use make SECURE=on [optionally SECURE_KEY ?= secure_boot_signing_key.pem] to enable Secure Boot and Flash Encryption mechanisms.)
endif # ifeq ($(SECURE), on)

# Default path to the project: we assume the Makefile including this file
# is in the project directory
ifndef PROJECT_PATH
PROJECT_PATH := $(abspath $(dir $(firstword $(MAKEFILE_LIST))))
endif

FROZEN_MPY_DIR = frozen

include ../py/mkenv.mk

CROSS_COMPILE = xtensa-esp32-elf-

ESP_IDF_PATH = $(IDF_PATH)
ESP_IDF_COMP_PATH = $(ESP_IDF_PATH)/components

CFLAGS_XTENSA = -DESP_PLATFORM -Wall -Werror=all -Wno-error=unused-function \
                -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wl,-EL \
                -mlongcalls -Wall -ffunction-sections -fdata-sections -fno-common \
                -Wno-sign-compare -Wno-old-style-declaration -fstrict-volatile-bitfields \
                -Wno-error=deprecated-declarations -Wno-unused-parameter -DWITH_POSIX \

CFLAGS_XTENSA_OPT = -Os
CFLAGS_XTENSA_PSRAM = -mfix-esp32-psram-cache-issue

CFLAGS = $(CFLAGS_XTENSA) $(CFLAGS_XTENSA_PSRAM) $(CFLAGS_XTENSA_OPT) -nostdlib -std=gnu99 -g3 -ggdb -fstrict-volatile-bitfields -Iboards/$(BOARD)
CFLAGS_SIGFOX = $(CFLAGS_XTENSA) -O2 -nostdlib -std=gnu99 -g3 -ggdb -fstrict-volatile-bitfields -Iboards/$(BOARD)

LDFLAGS = -nostdlib -Wl,-Map=$(@:.elf=.map) -Wl,--no-check-sections -u call_user_start_cpu0 \
          -mfix-esp32-psram-cache-issue \

LDFLAGS += -Wl,-static -Wl,--undefined=uxTopUsedPriority -Wl,--gc-sections

LIBS = -L$(ESP_IDF_COMP_PATH)/esp32/lib \
       -L$(ESP_IDF_COMP_PATH)/esp32/ld \
       -L$(ESP_IDF_COMP_PATH)/esp_rom/esp32/ld \
       -L$(ESP_IDF_COMP_PATH)/esp32/ld/wifi_iram_noopt \
       -L$(ESP_IDF_COMP_PATH)/bt/lib \
       -L$(ESP_IDF_COMP_PATH)/esp32 \
       -L$(ESP_IDF_COMP_PATH)/newlib/lib \
       -Lbootloader \
       -Llib \
       -L$(BUILD) \
       -lsigfox \
       lib/libc.a \
       lib/libm.a \
       lib/libgcc.a \
       lib/libstdc++.a \
       -lnvs_flash -ltcpip_adapter -llog -lcxx \
       -lspi_flash -lnvs_flash -lnghttp -ldriver -lesp32 -lexpat -lpthread -lesp_adc_cal \
       -lfreertos -ljson -ljsmn -llwip -lnewlib -lvfs -lopenssl -lmbedtls -lwpa_supplicant \
       -lbt -lsdmmc -lsoc -lheap -lbootloader_support \
       -lesp_ringbuf -lcoap -lmdns -lefuse -lespcoredump -lapp_update \
       -lesp_common -lesp_event -lesp_wifi -lphy -lbtdm_app -lrtc -lnet80211 -lxtensa -lpp -lcore -lmesh -lhal -lcoexist -lesp_eth -lsmartconfig

# These libraries cannot be used otherwise application.elf does not link, error: "undefined references to `__locale_ctype_ptr'"
# Using the libc and libm from the Toolchain solves this issue.
# In esp-idf 4.1 and 4.2 these libraries are completely removed from esp-idf...
#       $(ESP_IDF_COMP_PATH)/newlib/lib/libm-psram-workaround.a \
#       $(ESP_IDF_COMP_PATH)/newlib/lib/libc-psram-workaround.a \
#       $(ESP_IDF_COMP_PATH)/newlib/lib/libm.a \
#       $(ESP_IDF_COMP_PATH)/newlib/lib/libc.a \

ifeq ($(BOARD), $(filter $(BOARD), SIPY LOPY4 FIPY))
    LIBS += sigfox/modsigfox_$(BOARD).a
$(BUILD)/application.elf: sigfox/modsigfox_$(BOARD).a
endif

ifeq ($(OPENTHREAD), on)
    LIBS += -lopenthread
    CFLAGS += -DLORA_OPENTHREAD_ENABLED
endif

# Enable or Disable LTE_LOG_BUFF
ifeq ($(BOARD), $(filter $(BOARD), GPY FIPY))
ifeq ($(LTE_LOG_BUFF),1)
	CFLAGS += -DLTE_DEBUG_BUFF
endif #ifeq ($(LTE_LOG_BUFF),1)
endif #ifeq ($(BOARD), $(filter $(BOARD), GPY FIPY))
# Enable or Disable RGB led
ifeq ($(RGB_LED),disable)
	CFLAGS += -DRGB_LED_DISABLE
endif #ifeq ($(LTE_LOG_BUFF),1)

B_LIBS = bootloader/lib/libbootloader_support.a \
         bootloader/lib/libefuse.a \
         bootloader/lib/liblog.a \
         bootloader/lib/libmain.a \
         bootloader/lib/libmicro-ecc.a \
         bootloader/lib/libsoc.a \
         bootloader/lib/libspi_flash.a \
         lib/libgcc.a \

# objcopy paramters, to transform a binary file into an object file
OBJCOPY_EMBED_ARGS = --input-target binary --output-target elf32-xtensa-le --binary-architecture xtensa --rename-section .data=.rodata.embedded

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
