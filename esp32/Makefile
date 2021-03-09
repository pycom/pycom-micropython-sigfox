#
# Copyright © 2021, Pycom Limited.
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

IDF_HASH=3394ee5

TARGET ?= boot_app

OEM ?= 0

# make 'release' the default build type
BTYPE ?= release

# set to 1 to enable PyEthernet board (ksz8851 chip)
PYETH_ENABLED ?= 0

# set to 1 to enable LoRaWAN gateway (requires Pygate board)
PYGATE_ENABLED ?= 0

# COAP is enabled by default
MOD_COAP_ENABLED ?= 1

# LORA is enabled by default for supported boards
ifeq ($(BOARD), $(filter $(BOARD), LOPY LOPY4 FIPY))
MOD_LORA_ENABLED ?= 1
else
MOD_LORA_ENABLED ?= 0
endif

# SIGFOX is enabled by default for supported boards
ifeq ($(BOARD), $(filter $(BOARD), SIPY LOPY4 FIPY))
MOD_SIGFOX_ENABLED ?= 1
else
MOD_SIGFOX_ENABLED ?= 0
endif

# Pybytes disabled by default
PYBYTES_ENABLED ?= 0

# Flag to enable/disable Delta Update Feature. Disabled by default
DIFF_UPDATE_ENABLED ?= 0

# by default make a BASE firmware with features as specified with defaults above
# valid choices for VARIANT are: BASE, PYBYTES, PYGATE
VARIANT ?=BASE

ifeq ($(VARIANT),PYBYTES)
    PYBYTES_ENABLED=1
endif

ifeq ($(VARIANT),PYGATE)
    ifneq ($(BOARD), $(filter $(BOARD), WIPY GPY LOPY4 ))
        $(error PYGATE variant is not supported on $(BOARD))
    endif
    PYBYTES_ENABLED=1
    PYETH_ENABLED=1
    MOD_LORA_ENABLED=0 # ETH and LORA are mutually exclusive
    PYGATE_ENABLED=1
endif

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

CFLAGS_XTENSA = -DESP_PLATFORM -Wpointer-arith -Wall -Werror=all -Wno-error=unused-function \
                -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wl,-EL \
                -mlongcalls -Wall -ffunction-sections -fdata-sections -fno-common \
                -Wno-sign-compare -Wno-old-style-declaration -fstrict-volatile-bitfields \
                -Wno-error=deprecated-declarations -Wno-unused-parameter -DWITH_POSIX \

CFLAGS_XTENSA_OPT = -Os
CFLAGS_XTENSA_PSRAM = -mfix-esp32-psram-cache-issue

CFLAGS = $(CFLAGS_XTENSA) $(CFLAGS_XTENSA_PSRAM) $(CFLAGS_XTENSA_OPT) -nostdlib -std=gnu99 -g3 -ggdb -fstrict-volatile-bitfields -Iboards/$(BOARD)
CFLAGS += $(CFLAGS_MOD) $(CFLAGS_EXTRA)
CFLAGS_SIGFOX = $(CFLAGS_XTENSA) -O2 -nostdlib -std=gnu99 -g3 -ggdb -fstrict-volatile-bitfields -Iboards/$(BOARD)

# Configure floating point support
ifeq ($(MICROPY_FLOAT_IMPL),double)
CFLAGS += -DMICROPY_FLOAT_IMPL=MICROPY_FLOAT_IMPL_DOUBLE
else
ifeq ($(MICROPY_FLOAT_IMPL),none)
CFLAGS += -DMICROPY_FLOAT_IMPL=MICROPY_FLOAT_IMPL_NONE
else
ifeq ($(MICROPY_FLOAT_IMPL),single)
CFLAGS += -DMICROPY_FLOAT_IMPL=MICROPY_FLOAT_IMPL_FLOAT
endif
endif
endif

LDFLAGS = -nostdlib -Wl,-Map=$(@:.elf=.map) -Wl,--no-check-sections -u call_user_start_cpu0
LDFLAGS += -Wl,-static -Wl,--undefined=uxTopUsedPriority -Wl,--gc-sections

LIBS = -L$(ESP_IDF_COMP_PATH)/esp32/lib -L$(ESP_IDF_COMP_PATH)/esp32/ld -L$(ESP_IDF_COMP_PATH)/esp32/ld/wifi_iram_noopt -L$(ESP_IDF_COMP_PATH)/bt/lib \
       -L$(ESP_IDF_COMP_PATH)/esp32 -L$(ESP_IDF_COMP_PATH)/newlib/lib -Lbootloader \
       -Llib -lnvs_flash -ltcpip_adapter -L$(BUILD) -lhal -lcore -lwps -lcoexist -lstdc++ \
       -lnet80211 -lphy -lpp -lrtc -llog -lsmartconfig -lwpa -lwpa2 -lbtdm_app -lcxx -u __cxa_guard_dummy \
       -lspi_flash -lnvs_flash -lgcc -lnghttp -ldriver -lesp32 -lexpat -lpthread -lesp_adc_cal \
       $(ESP_IDF_COMP_PATH)/newlib/lib/libm-psram-workaround.a \
       $(ESP_IDF_COMP_PATH)/newlib/lib/libc-psram-workaround.a \
       -lfreertos -ljson -ljsmn -llwip -lnewlib -lvfs -lopenssl -lmbedtls -lwpa_supplicant \
       -lxtensa-debug-module -lbt -lsdmmc -lsoc -lheap -lbootloader_support -lmicro-ecc \
       -u ld_include_panic_highint_hdl -lsmartconfig_ack -lmesh -lesp_ringbuf -lmdns -lefuse -lespcoredump -lapp_update

ifeq ($(MOD_COAP_ENABLED), 1)
    $(info COAP Module Enabled)
    LIBS += -lcoap
    CFLAGS += -DMOD_COAP_ENABLED
endif

ifeq ($(MOD_LORA_ENABLED), 1)
    $(info LORA Module Enabled)
    CFLAGS += -DMOD_LORA_ENABLED
endif

ifeq ($(DIFF_UPDATE_ENABLED), 1)
    $(info Differential Update Enabled)
    CFLAGS += -DDIFF_UPDATE_ENABLED -DBZ_NO_STDIO
endif

ifeq ($(MOD_SIGFOX_ENABLED), 1)
    $(info SIGFOX Module Enabled)
    CFLAGS += -DMOD_SIGFOX_ENABLED
    LIBS += sigfox/modsigfox_$(BOARD).a -lsigfox
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

B_LIBS = -Lbootloader/lib -Lbootloader -L$(BUILD)/bootloader -L$(ESP_IDF_COMP_PATH)/esp32/ld \
         -L$(ESP_IDF_COMP_PATH)/esp32/lib -llog -lcore -lbootloader_support \
         -lspi_flash -lsoc -lmicro-ecc -lgcc -lstdc++ -lgcov -lefuse

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
