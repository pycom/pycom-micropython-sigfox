#
# Copyright (c) 2016, Pycom Limited.
#
# This software is licensed under the GNU GPL version 3 or any
# later version, with permitted additional terms. For more information
# see the Pycom Licence v1.0 document supplied with this file, or
# available at https://www.pycom.io/opensource/licensing
#

APP_INC =  -I.
APP_INC += -I..
APP_INC += -Ihal
APP_INC += -Iutil
APP_INC += -Imods
APP_INC += -Itelnet
APP_INC += -Iftp
APP_INC += -Ilora
APP_INC += -Ibootloader
APP_INC += -Ifatfs/src/drivers
APP_INC += -I$(BUILD)
APP_INC += -I$(BUILD)/genhdr
APP_INC += -I$(ESP_IDF_COMP_PATH)/bootloader_support/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bootloader_support/include_priv
APP_INC += -I$(ESP_IDF_COMP_PATH)/mbedtls/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/mbedtls/port/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/driver/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/driver/include/driver
APP_INC += -I$(ESP_IDF_COMP_PATH)/esp32
APP_INC += -I$(ESP_IDF_COMP_PATH)/esp32/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/expat/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/freertos/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/json/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/expat/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/lwip/include/lwip
APP_INC += -I$(ESP_IDF_COMP_PATH)/lwip/include/lwip/port
APP_INC += -I$(ESP_IDF_COMP_PATH)/newlib/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/nvs_flash/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/spi_flash/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/tcpip_adapter/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/log/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/sdmmc/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/device/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/bta/dm
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/bta/hh
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/bta/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/bta/sys/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/stack/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/stack/gatt/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/stack/gap/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/stack/l2cap/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/btcore/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/osi/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/hci/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/gki/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/api/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/btc/include
APP_INC += -I../lib/mp-readline
APP_INC += -I../lib/netutils
APP_INC += -I../lib/fatfs
APP_INC += -I../lib
APP_INC += -I../drivers/sx127x
APP_INC += -I../stmhal

APP_MAIN_SRC_C = \
	main.c \
	mptask.c \
	serverstask.c \
	pycom_config.c \
	mpthreadport.c \

APP_HAL_SRC_C = $(addprefix hal/,\
	esp32_mphal.c \
	)

APP_LIB_SRC_C = $(addprefix lib/,\
	libm/math.c \
	libm/fmodf.c \
	libm/roundf.c \
	libm/ef_sqrt.c \
	libm/kf_rem_pio2.c \
	libm/kf_sin.c \
	libm/kf_cos.c \
	libm/kf_tan.c \
	libm/ef_rem_pio2.c \
	libm/sf_sin.c \
	libm/sf_cos.c \
	libm/sf_tan.c \
	libm/sf_frexp.c \
	libm/sf_modf.c \
	libm/sf_ldexp.c \
	libm/asinfacosf.c \
	libm/atanf.c \
	libm/atan2f.c \
	mp-readline/readline.c \
	netutils/netutils.c \
	utils/pyexec.c \
	utils/interrupt_char.c \
	fatfs/ff.c \
	fatfs/option/ccsbcs.c \
	)

ifeq ($(BOARD), LOPY)
APP_MODS_SRC_C = $(addprefix mods/,\
	machuart.c \
	machpin.c \
	machrtc.c \
	machspi.c \
	machine_i2c.c \
	machpwm.c \
	modmachine.c \
	moduos.c \
	modusocket.c \
	modnetwork.c \
	modwlan.c \
	moduselect.c \
	modutime.c \
	modlora.c \
	modpycom.c \
	moduhashlib.c \
	moducrypto.c \
	machtimer.c \
	machtimer_alarm.c \
	machtimer_chrono.c \
	analog.c \
	pybadc.c \
	pybdac.c \
	pybsd.c \
	modussl.c \
	modbt.c \
	modled.c \
	)
endif

ifeq ($(BOARD), WIPY)
APP_MODS_SRC_C = $(addprefix mods/,\
	machuart.c \
	machpin.c \
	machrtc.c \
	machspi.c \
	machine_i2c.c \
	machpwm.c \
	modmachine.c \
	moduos.c \
	modusocket.c \
	modnetwork.c \
	modwlan.c \
	moduselect.c \
	modutime.c \
	modpycom.c \
	moduhashlib.c \
	moducrypto.c \
	machtimer.c \
	machtimer_alarm.c \
	machtimer_chrono.c \
	analog.c \
	pybadc.c \
	pybdac.c \
	pybsd.c \
	modussl.c \
	modbt.c \
	modled.c \
	)
endif

APP_STM_SRC_C = $(addprefix stmhal/,\
	bufhelper.c \
	builtin_open.c \
	import.c \
	input.c \
	lexerfatfs.c \
	pybstdio.c \
	)

APP_UTIL_SRC_C = $(addprefix util/,\
	antenna.c \
	btdynmem.c \
	gccollect.c \
	help.c \
	mperror.c \
	random.c \
	mpexception.c \
	fifo.c \
	socketfifo.c \
	mpirq.c \
	mpsleep.c \
	timeutils.c \
	)

APP_FATFS_SRC_C = $(addprefix fatfs/src/,\
	drivers/sflash_diskio.c \
	drivers/sd_diskio.c \
	option/syscall.c \
	diskio.c \
	ffconf.c \
	)

APP_LORA_SRC_C = $(addprefix lora/,\
	utilities.c \
	timer-board.c \
	gpio-board.c \
	spi-board.c \
	sx1272-board.c \
	board.c \
	)

APP_LIB_LORA_SRC_C = $(addprefix lib/lora/,\
	mac/LoRaMac.c \
	mac/LoRaMacCrypto.c \
	system/delay.c \
	system/gpio.c \
	system/timer.c \
	system/crypto/aes.c \
	system/crypto/cmac.c \
	)

APP_SX1272_SRC_C = $(addprefix drivers/sx127x/,\
	sx1272/sx1272.c \
	)

APP_TELNET_SRC_C = $(addprefix telnet/,\
	telnet.c \
	)

APP_FTP_SRC_C = $(addprefix ftp/,\
	ftp.c \
	updater.c \
	)

BOOT_SRC_C = $(addprefix bootloader/,\
	bootloader.c \
	bootmgr.c \
	mperror.c \
	gpio.c \
	)

OBJ = $(PY_O)
OBJ += $(addprefix $(BUILD)/, $(APP_MAIN_SRC_C:.c=.o) $(APP_HAL_SRC_C:.c=.o) $(APP_LIB_SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(APP_MODS_SRC_C:.c=.o) $(APP_STM_SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(APP_FATFS_SRC_C:.c=.o) $(APP_UTIL_SRC_C:.c=.o) $(APP_TELNET_SRC_C:.c=.o))
ifeq ($(BOARD), LOPY)
OBJ += $(addprefix $(BUILD)/, $(APP_LORA_SRC_C:.c=.o) $(APP_LIB_LORA_SRC_C:.c=.o) $(APP_SX1272_SRC_C:.c=.o))
endif
OBJ += $(addprefix $(BUILD)/, $(APP_FTP_SRC_C:.c=.o))
OBJ += $(BUILD)/pins.o

BOOT_OBJ = $(addprefix $(BUILD)/, $(BOOT_SRC_C:.c=.o))

# List of sources for qstr extraction
SRC_QSTR += $(APP_MODS_SRC_C) $(APP_UTIL_SRC_C) $(APP_STM_SRC_C)
# Append any auto-generated sources that are needed by sources listed in
# SRC_QSTR
SRC_QSTR_AUTO_DEPS +=

BOOT_LDFLAGS = $(LDFLAGS) -T esp32.bootloader.ld -T esp32.rom.ld -T esp32.peripherals.ld

# add the application linker script(s)
APP_LDFLAGS += $(LDFLAGS) -T esp32_out.ld -T esp32.common.ld -T esp32.rom.ld -T esp32.peripherals.ld

LORA_BAND ?= USE_BAND_868
ifeq ($(BOARD), LOPY)
    ifeq ($(LORA_BAND), USE_BAND_868)
        LORA_FREQ = 868
    else
        LORA_FREQ = 915
    endif
else
    LORA_FREQ =
endif

# add the application specific CFLAGS
CFLAGS += $(APP_INC) -DMICROPY_NLR_SETJMP=1 -D$(LORA_BAND) -DMBEDTLS_CONFIG_FILE='"mbedtls/esp_config.h"' -DHAVE_CONFIG_H -DESP_PLATFORM

# add the application archive, this order is very important
APP_LIBS = -Wl,--start-group $(LIBS) $(BUILD)/application.a -Wl,--end-group -Wl,-EL

BOOT_LIBS = -Wl,--start-group $(B_LIBS) $(BUILD)/bootloader/bootloader.a -Wl,--end-group -Wl,-EL

# debug / optimization options
ifeq ($(BTYPE), debug)
    CFLAGS += -DDEBUG_B -DNDEBUG
else
    ifeq ($(BTYPE), release)
        CFLAGS += -DNDEBUG
    else
        $(error Invalid BTYPE specified)
    endif
endif

$(BUILD)/bootloader/%.o: CFLAGS += -DBOOTLOADER_BUILD

BOOT_OFFSET = 0x1000
PART_OFFSET = 0x8000
APP_OFFSET  = 0x10000

SHELL    = bash
APP_SIGN = tools/appsign.sh

BOOT_BIN = $(BUILD)/bootloader/bootloader.bin
ifeq ($(BOARD), LOPY)
    APP_BIN  = $(BUILD)/lopy_$(LORA_FREQ).bin
else
    APP_BIN  = $(BUILD)/wipy.bin
endif
APP_IMG  = $(BUILD)/appimg.bin
PART_CSV = lib/partitions.csv
PART_BIN = $(BUILD)/lib/partitions.bin

ESPPORT ?= /dev/ttyUSB0
ESPBAUD ?= 921600

FLASH_SIZE = 4MB

ESPFLASHMODE = qio
ESPFLASHFREQ = 40m
ESPTOOLPY = $(PYTHON) $(IDF_PATH)/components/esptool_py/esptool/esptool.py --chip esp32
ESPTOOLPY_SERIAL = $(ESPTOOLPY) --port $(ESPPORT) --baud $(ESPBAUD)

ESPTOOLPY_WRITE_FLASH  = $(ESPTOOLPY_SERIAL) write_flash -z --flash_mode $(ESPFLASHMODE) --flash_freq $(ESPFLASHFREQ) --flash_size $(FLASH_SIZE)
ESPTOOLPY_ERASE_FLASH  = $(ESPTOOLPY_SERIAL) erase_flash
ESPTOOL_ALL_FLASH_ARGS = $(BOOT_OFFSET) $(BOOT_BIN) $(PART_OFFSET) $(PART_BIN) $(APP_OFFSET) $(APP_BIN)

GEN_ESP32PART := $(PYTHON) $(ESP_IDF_COMP_PATH)/partition_table/gen_esp32part.py -q

BOOT_BIN = $(BUILD)/bootloader/bootloader.bin

all: $(BOOT_BIN) $(APP_BIN)

.PHONY: all

$(BUILD)/bootloader/bootloader.a: $(BOOT_OBJ) sdkconfig.h
	$(ECHO) "AR $@"
	$(Q) rm -f $@
	$(Q) $(AR) cru $@ $^

$(BUILD)/bootloader/bootloader.elf: $(BUILD)/bootloader/bootloader.a
	$(ECHO) "LINK $@"
	$(Q) $(CC) $(BOOT_LDFLAGS) $(BOOT_LIBS) -o $@
	$(Q) $(SIZE) $@

$(BOOT_BIN): $(BUILD)/bootloader/bootloader.elf
	$(ECHO) "IMAGE $@"
	$(Q) $(ESPTOOLPY) elf2image --flash_mode $(ESPFLASHMODE) --flash_freq $(ESPFLASHFREQ) --flash_size $(FLASH_SIZE) -o $@ $<

$(BUILD)/application.a: $(OBJ)
	$(ECHO) "AR $@"
	$(Q) rm -f $@
	$(Q) $(AR) cru $@ $^

$(BUILD)/application.elf: $(BUILD)/application.a $(BUILD)/esp32_out.ld
	$(ECHO) "LINK $@"
	$(Q) $(CC) $(APP_LDFLAGS) $(APP_LIBS) -o $@
	$(Q) $(SIZE) $@

$(APP_BIN): $(BUILD)/application.elf $(PART_BIN)
	$(ECHO) "IMAGE $@"
	$(Q) $(ESPTOOLPY) elf2image --flash_mode $(ESPFLASHMODE) --flash_freq $(ESPFLASHFREQ) -o $@ $<
	$(ECHO) "Signing OTA image"
	$(Q)$(SHELL) $(APP_SIGN) $(APP_BIN) $(BUILD)

$(BUILD)/esp32_out.ld: $(ESP_IDF_COMP_PATH)/esp32/ld/esp32.ld sdkconfig.h
	$(ECHO) "CPP $@"
	$(Q) $(CC) -I. -C -P -x c -E $< -o $@

flash: $(APP_BIN) $(BOOT_BIN)
	$(ECHO) "Flashing project"
	$(Q) $(ESPTOOLPY_WRITE_FLASH) $(ESPTOOL_ALL_FLASH_ARGS)

erase:
	$(ECHO) "Erasing flash"
	$(Q) $(ESPTOOLPY_ERASE_FLASH)

$(PART_BIN): $(PART_CSV)
	$(ECHO) "Building partitions from $(PART_CSV)..."
	$(Q) $(GEN_ESP32PART) $< $@

show_partitions: $(PART_BIN)
	$(ECHO) "Partition table binary generated. Contents:"
	$(ECHO) $(SEPARATOR)
	$(Q) $(GEN_ESP32PART) $<
	$(ECHO) $(SEPARATOR)

MAKE_PINS = boards/make-pins.py
BOARD_PINS = boards/$(BOARD)/pins.csv
AF_FILE = boards/esp32_af.csv
PREFIX_FILE = boards/esp32_prefix.c
GEN_PINS_SRC = $(BUILD)/pins.c
GEN_PINS_HDR = $(HEADER_BUILD)/pins.h
GEN_PINS_QSTR = $(BUILD)/pins_qstr.h

# Making OBJ use an order-only dependency on the generated pins.h file
# has the side effect of making the pins.h file before we actually compile
# any of the objects. The normal dependency generation will deal with the
# case when pins.h is modified. But when it doesn't exist, we don't know
# which source files might need it.
$(OBJ): | $(GEN_PINS_HDR)

# Call make-pins.py to generate both pins_gen.c and pins.h
$(GEN_PINS_SRC) $(GEN_PINS_HDR) $(GEN_PINS_QSTR): $(BOARD_PINS) $(MAKE_PINS) $(AF_FILE) $(PREFIX_FILE) | $(HEADER_BUILD)
	$(ECHO) "Create $@"
	$(Q)$(PYTHON) $(MAKE_PINS) --board $(BOARD_PINS) --af $(AF_FILE) --prefix $(PREFIX_FILE) --hdr $(GEN_PINS_HDR) --qstr $(GEN_PINS_QSTR) > $(GEN_PINS_SRC)

$(BUILD)/pins.o: $(BUILD)/pins.c
	$(call compile_c)
