#
# Copyright (c) 2021, Pycom Limited.
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
APP_INC += -Ilte
APP_INC += -Ican
APP_INC += -Ibootloader
APP_INC += -Ifatfs/src/drivers
ifeq ($(PYGATE_ENABLED), 1)
APP_INC += -Ipygate/concentrator
APP_INC += -Ipygate/hal/include
APP_INC += -Ipygate/lora_pkt_fwd
APP_INC += -I$(ESP_IDF_COMP_PATH)/pthread/include/
APP_INC += -I$(ESP_IDF_COMP_PATH)/sntp/include/
endif
APP_INC += -Ilittlefs
APP_INC += -I$(BUILD)
APP_INC += -I$(BUILD)/genhdr
APP_INC += -I$(ESP_IDF_COMP_PATH)/bootloader_support/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bootloader_support/include_priv
APP_INC += -I$(ESP_IDF_COMP_PATH)/bootloader_support/include_bootloader
APP_INC += -I$(ESP_IDF_COMP_PATH)/mbedtls/mbedtls/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/mbedtls/port/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/driver/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/driver/include/driver
APP_INC += -I$(ESP_IDF_COMP_PATH)/heap/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/esp32
APP_INC += -I$(ESP_IDF_COMP_PATH)/esp32/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/esp_ringbuf/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/esp_event/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/esp_adc_cal/include
ifeq ($(PYETH_ENABLED), 1)
APP_INC += -I$(ESP_IDF_COMP_PATH)/ethernet/include
endif
APP_INC += -I$(ESP_IDF_COMP_PATH)/soc/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/soc/esp32/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/expat/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/freertos/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/json/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/expat/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/lwip/include/lwip
APP_INC += -I$(ESP_IDF_COMP_PATH)/lwip/include/lwip/port
APP_INC += -I$(ESP_IDF_COMP_PATH)/lwip/include/apps
APP_INC += -I$(ESP_IDF_COMP_PATH)/lwip/port/esp32/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/lwip/lwip/src/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/lwip/include/lwip/posix
APP_INC += -I$(ESP_IDF_COMP_PATH)/newlib/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/newlib/platform_include
APP_INC += -I$(ESP_IDF_COMP_PATH)/nvs_flash/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/spi_flash/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/tcpip_adapter/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/log/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/sdmmc/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/vfs/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/device/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/device/include/device
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/bta/dm
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/bta/hh
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/bta/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/bta/sys/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/bt/bluedroid/common/include
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

ifeq ($(MOD_COAP_ENABLED), 1)
APP_INC += -I$(ESP_IDF_COMP_PATH)/coap/libcoap/include/coap
APP_INC += -I$(ESP_IDF_COMP_PATH)/coap/libcoap/examples
APP_INC += -I$(ESP_IDF_COMP_PATH)/coap/port/include
APP_INC += -I$(ESP_IDF_COMP_PATH)/coap/port/include/coap
endif

APP_INC += -I$(ESP_IDF_COMP_PATH)/mdns/include
APP_INC += -I../lib/mp-readline
APP_INC += -I../lib/netutils
APP_INC += -I../lib/oofatfs
APP_INC += -I../lib
APP_INC += -I../drivers/sx127x
ifeq ($(PYGATE_ENABLED), 1)
APP_INC += -I../drivers/sx1308
endif
ifeq ($(PYETH_ENABLED), 1)
APP_INC += -I../drivers/ksz8851
endif
APP_INC += -I../ports/stm32
APP_INC += -I$(ESP_IDF_COMP_PATH)/openthread/src

APP_MAIN_SRC_C = \
	main.c \
	mptask.c \
	serverstask.c \
	fatfs_port.c \
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
	utils/sys_stdio_mphal.c \
	oofatfs/ff.c \
	oofatfs/ffunicode.c \
	timeutils/timeutils.c \
	)

APP_MODS_SRC_C = $(addprefix mods/,\
	machuart.c \
	machpin.c \
	machrtc.c \
	pybflash.c \
	machspi.c \
	machine_i2c.c \
	machpwm.c \
	machcan.c \
	modmachine.c \
	moduos.c \
	modusocket.c \
	modnetwork.c \
	modwlan.c \
	modutime.c \
	modpycom.c \
	moduqueue.c \
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
	machwdt.c \
	machrmt.c \
	lwipsocket.c \
	machtouch.c \
	modmdns.c \
	)
ifeq ($(MOD_COAP_ENABLED), 1)
APP_INC += -Ibsdiff
APP_MODS_SRC_C += $(addprefix mods/,\
	modcoap.c \
	)
endif

ifeq ($(DIFF_UPDATE_ENABLED), 1)
APP_INC += -Ibzlib/
APP_MODS_SRC_C += $(addprefix bsdiff/,\
	bspatch.c \
	)
APP_MODS_SRC_C += $(addprefix bzlib/,\
	blocksort.c \
	huffman.c \
	crctable.c \
	randtable.c \
	compress.c \
	decompress.c \
	bzlib.c \
	bzlib_ext.c \
	)
endif

APP_MODS_LORA_SRC_C = $(addprefix mods/,\
	modlora.c \
	)

APP_STM_SRC_C = $(addprefix ports/stm32/,\
	bufhelper.c \
	)

APP_UTIL_SRC_C = $(addprefix util/,\
	antenna.c \
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
	esp32chipinfo.c \
	pycom_general_util.c \
	str_utils.c \
	)

APP_FATFS_SRC_C = $(addprefix fatfs/src/,\
	drivers/sflash_diskio.c \
	drivers/sd_diskio.c \
	)

APP_LITTLEFS_SRC_C = $(addprefix littlefs/,\
	lfs.c \
	lfs_util.c \
	vfs_littlefs.c \
	vfs_littlefs_file.c \
	sflash_diskio_littlefs.c \
	)

APP_LORA_SRC_C = $(addprefix lora/,\
	utilities.c \
	timer-board.c \
	gpio-board.c \
	spi-board.c \
	sx1276-board.c \
	sx1272-board.c \
	board.c \
	)

APP_LORA_OPENTHREAD_SRC_C = $(addprefix lora/,\
	otplat_alarm.c \
	otplat_radio.c \
	ot-settings.c \
	ot-log.c \
	)

APP_MOD_MESH_SRC_C = $(addprefix mods/,\
	modmesh.c \
	)

APP_LIB_LORA_SRC_C = $(addprefix lib/lora/,\
	mac/LoRaMac.c \
	mac/LoRaMacCrypto.c \
	mac/region/Region.c \
	mac/region/RegionAS923.c \
	mac/region/RegionAU915.c \
	mac/region/RegionCommon.c \
	mac/region/RegionEU868.c \
	mac/region/RegionUS915.c \
	mac/region/RegionCN470.c \
	mac/region/RegionEU433.c \
	mac/region/RegionIN865.c \
	system/delay.c \
	system/gpio.c \
	system/timer.c \
	system/crypto/aes.c \
	system/crypto/cmac.c \
	)

APP_SX1308_SRC_C = $(addprefix drivers/sx1308/,\
	sx1308.c \
	sx1308-spi.c \
	)

APP_PYGATE_SRC_C = $(addprefix pygate/,\
	concentrator/loragw_reg_esp.c \
	concentrator/loragw_hal_esp.c \
	concentrator/cmd_manager.c \
	hal/loragw_aux.c \
	hal/loragw_mcu.c \
	hal/loragw_com_esp.c \
	hal/loragw_com.c \
	hal/loragw_hal.c \
	hal/loragw_radio.c \
	hal/loragw_reg.c \
	lora_pkt_fwd/base64.c \
	lora_pkt_fwd/jitqueue.c \
	lora_pkt_fwd/lora_pkt_fwd.c \
	lora_pkt_fwd/parson.c \
	lora_pkt_fwd/timersync.c \
	)

APP_ETHERNET_SRC_C = $(addprefix mods/,\
	modeth.c \
	)

APP_SX1272_SRC_C = $(addprefix drivers/sx127x/,\
	sx1272/sx1272.c \
	)

APP_SX1276_SRC_C = $(addprefix drivers/sx127x/,\
	sx1276/sx1276.c \
	)

APP_KSZ8851_SRC_C = $(addprefix drivers/ksz8851/,\
	ksz8851.c \
	)

APP_SIGFOX_SRC_SIPY_C = $(addprefix sigfox/src/,\
	manufacturer_api.c \
	radio.c \
	ti_aes_128.c \
	timer.c \
	transmission.c \
	modsigfox.c \
	)

APP_SIGFOX_SRC_FIPY_LOPY4_C = $(addprefix sigfox/src/,\
	manufacturer_api.c \
	radio_sx127x.c \
	ti_aes_128.c \
	timer.c \
	transmission.c \
	modsigfox.c \
	)

APP_SIGFOX_MOD_SRC_C = $(addprefix mods/,\
	modsigfox_api.c \
	)

APP_SIGFOX_TARGET_SRC_C = $(addprefix sigfox/src/targets/,\
	cc112x_spi.c \
	hal_int.c \
	hal_spi_rf_trxeb.c \
	trx_rf_int.c \
	)

APP_SIGFOX_SPI_SRC_C = $(addprefix lora/,\
	spi-board.c \
	gpio-board.c \
	)

APP_LTE_SRC_C = $(addprefix lte/,\
    lteppp.c \
    )

APP_MODS_LTE_SRC_C = $(addprefix mods/,\
    modlte.c \
    )

APP_TELNET_SRC_C = $(addprefix telnet/,\
	telnet.c \
	)

APP_FTP_SRC_C = $(addprefix ftp/,\
	ftp.c \
	updater.c \
	)

APP_CAN_SRC_C = $(addprefix can/,\
	CAN.c \
	)

BOOT_SRC_C = $(addprefix bootloader/,\
	bootloader.c \
	bootmgr.c \
	mperror.c \
	gpio.c \
	)

SFX_OBJ =

OBJ = $(PY_O)
ifeq ($(MOD_LORA_ENABLED), 1)

ifeq ($(BOARD), $(filter $(BOARD), LOPY FIPY))
OBJ += $(addprefix $(BUILD)/, $(APP_LORA_SRC_C:.c=.o) $(APP_LIB_LORA_SRC_C:.c=.o) $(APP_SX1272_SRC_C:.c=.o) $(APP_MODS_LORA_SRC_C:.c=.o))
endif

ifeq ($(BOARD), $(filter $(BOARD), LOPY4))
OBJ += $(addprefix $(BUILD)/, $(APP_LORA_SRC_C:.c=.o) $(APP_LIB_LORA_SRC_C:.c=.o) $(APP_SX1276_SRC_C:.c=.o) $(APP_MODS_LORA_SRC_C:.c=.o))
endif

endif

ifeq ($(MOD_SIGFOX_ENABLED), 1)

ifeq ($(BOARD), $(filter $(BOARD), LOPY FIPY))
OBJ += $(addprefix $(BUILD)/, $(APP_LORA_SRC_C:.c=.o) $(APP_LIB_LORA_SRC_C:.c=.o) $(APP_SX1272_SRC_C:.c=.o) $(APP_MODS_LORA_SRC_C:.c=.o))
endif

ifeq ($(BOARD), $(filter $(BOARD), LOPY4))
OBJ += $(addprefix $(BUILD)/, $(APP_LORA_SRC_C:.c=.o) $(APP_LIB_LORA_SRC_C:.c=.o) $(APP_SX1276_SRC_C:.c=.o) $(APP_MODS_LORA_SRC_C:.c=.o))
endif

endif

ifeq ($(MOD_SIGFOX_ENABLED), 1)
ifeq ($(BOARD), $(filter $(BOARD), SIPY))
OBJ += $(addprefix $(BUILD)/, $(APP_SIGFOX_MOD_SRC_C:.c=.o))
endif
ifeq ($(BOARD), $(filter $(BOARD), LOPY4 FIPY))
OBJ += $(addprefix $(BUILD)/, $(APP_SIGFOX_MOD_SRC_C:.c=.o))
endif
endif

ifeq ($(BOARD),$(filter $(BOARD), FIPY GPY))
OBJ += $(addprefix $(BUILD)/, $(APP_LTE_SRC_C:.c=.o) $(APP_MODS_LTE_SRC_C:.c=.o))
endif

# add OPENTHREAD code only if flag enabled and for LOPY, LOPY4 and FIPY
ifeq ($(OPENTHREAD), on)
ifeq ($(BOARD), $(filter $(BOARD), LOPY LOPY4 FIPY))
OBJ += $(addprefix $(BUILD)/, $(APP_LORA_OPENTHREAD_SRC_C:.c=.o) $(APP_MOD_MESH_SRC_C:.c=.o))
endif
endif # ifeq ($(OPENTHREAD), on)

OBJ += $(addprefix $(BUILD)/, $(APP_MAIN_SRC_C:.c=.o) $(APP_HAL_SRC_C:.c=.o) $(APP_LIB_SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(APP_MODS_SRC_C:.c=.o) $(APP_STM_SRC_C:.c=.o) $(SRC_MOD:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(APP_FATFS_SRC_C:.c=.o) $(APP_LITTLEFS_SRC_C:.c=.o) $(APP_UTIL_SRC_C:.c=.o) $(APP_TELNET_SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(APP_FTP_SRC_C:.c=.o) $(APP_CAN_SRC_C:.c=.o))
ifeq ($(PYGATE_ENABLED), 1)
$(info Pygate Enabled)
OBJ += $(addprefix $(BUILD)/, $(APP_SX1308_SRC_C:.c=.o) $(APP_PYGATE_SRC_C:.c=.o))
CFLAGS += -DPYGATE_ENABLED
SRC_QSTR += $(APP_SX1308_SRC_C) $(APP_PYGATE_SRC_C)
endif
ifeq ($(PYETH_ENABLED), 1)
$(info PyEthernet Enabled)
OBJ += $(addprefix $(BUILD)/, $(APP_KSZ8851_SRC_C:.c=.o) $(APP_ETHERNET_SRC_C:.c=.o))
CFLAGS += -DPYETH_ENABLED
SRC_QSTR += $(APP_KSZ8851_SRC_C) $(APP_ETHERNET_SRC_C)
endif
OBJ += $(BUILD)/pins.o

BOOT_OBJ = $(addprefix $(BUILD)/, $(BOOT_SRC_C:.c=.o))

# List of sources for qstr extraction
SRC_QSTR += $(APP_MODS_SRC_C) $(APP_UTIL_SRC_C) $(APP_STM_SRC_C) $(APP_LIB_SRC_C) $(SRC_MOD) 
ifeq ($(BOARD), $(filter $(BOARD), LOPY LOPY4 FIPY))
SRC_QSTR += $(APP_MODS_LORA_SRC_C)
endif
ifeq ($(BOARD), $(filter $(BOARD), SIPY LOPY4 FIPY))
ifeq ($(MOD_SIGFOX_ENABLED), 1)
SRC_QSTR += $(APP_SIGFOX_MOD_SRC_C)
endif
endif
ifeq ($(BOARD),$(filter $(BOARD), FIPY GPY))
SRC_QSTR += $(APP_MODS_LTE_SRC_C)
endif

ifeq ($(OPENTHREAD), on)
ifeq ($(BOARD), $(filter $(BOARD), LOPY LOPY4 FIPY))
SRC_QSTR += $(APP_MOD_MESH_SRC_C)
endif
endif # ifeq ($(OPENTHREAD), on)

# Append any auto-generated sources that are needed by sources listed in
# SRC_QSTR
SRC_QSTR_AUTO_DEPS +=

BOOT_LDFLAGS = $(LDFLAGS) -T esp32.bootloader.ld -T esp32.rom.ld -T esp32.peripherals.ld -T esp32.bootloader.rom.ld -T esp32.rom.spiram_incompatible_fns.ld -T esp32.extram.bss.ld

# add the application linker script(s)
APP_LDFLAGS += $(LDFLAGS) -T esp32_out.ld -T esp32.project.ld -T esp32.rom.ld -T esp32.peripherals.ld -T esp32.rom.libgcc.ld -T esp32.extram.bss.ld
APP_LDFLAGS += $(LDFLAGS_MOD)
# add the application specific CFLAGS
CFLAGS += $(APP_INC) -DMICROPY_NLR_SETJMP=1 -DMBEDTLS_CONFIG_FILE='"mbedtls/esp_config.h"' -DHAVE_CONFIG_H -DESP_PLATFORM -DFFCONF_H=\"lib/oofatfs/ffconf.h\" -DWITH_POSIX
CFLAGS_SIGFOX += $(APP_INC) -DMICROPY_NLR_SETJMP=1 -DMBEDTLS_CONFIG_FILE='"mbedtls/esp_config.h"' -DHAVE_CONFIG_H -DESP_PLATFORM
CFLAGS += -DREGION_AS923 -DREGION_AU915 -DREGION_EU868 -DREGION_US915 -DREGION_CN470 -DREGION_EU433 -DREGION_IN865 -DBASE=0 -DPYBYTES=1 

# Specify if this is Firmware build has Pybytes enabled
ifeq ($(PYBYTES_ENABLED), 1)
$(info Pybytes Enabled)
CFLAGS += -DVARIANT=1
else
CFLAGS += -DVARIANT=0
endif

# Give the possibility to use LittleFs on /flash, otherwise FatFs is used
FS ?= ""
ifeq ($(FS), LFS)
    CFLAGS += -DFS_USE_LITTLEFS
endif

# add the application archive, this order is very important
APP_LIBS = -Wl,--start-group $(LIBS) $(BUILD)/application.a -Wl,--end-group -Wl,-EL

BOOT_LIBS = -Wl,--start-group $(B_LIBS) $(BUILD)/bootloader/bootloader.a -Wl,--end-group -Wl,-EL

# debug / optimization options
ifeq ($(BTYPE), debug)
    CFLAGS += -DDEBUG
    CFLAGS_SIGFOX += -DDEBUG
else
    ifeq ($(BTYPE), release)
        CFLAGS += -DNDEBUG
        CFLAGS_SIGFOX += -DNDEBUG
    else
        $(error Invalid BTYPE specified)
    endif
endif

$(BUILD)/bootloader/%.o: CFLAGS += -D BOOTLOADER_BUILD=1
$(BUILD)/bootloader/%.o: CFLAGS_SIGFOX += -D BOOTLOADER_BUILD=1

BOOT_OFFSET = 0x1000
PART_OFFSET = 0x8000
APP_OFFSET  = 0x10000

SHELL    = bash

BOOT_BIN = $(BUILD)/bootloader/bootloader.bin

ifeq ($(BOARD), WIPY)
    APP_BIN = $(BUILD)/wipy.bin
endif
ifeq ($(BOARD), LOPY)
    APP_BIN = $(BUILD)/lopy.bin
endif
ifeq ($(BOARD), LOPY4)
    APP_BIN = $(BUILD)/lopy4.bin
    ifeq ($(MOD_SIGFOX_ENABLED), 1)
        $(BUILD)/sigfox/radio_sx127x.o: CFLAGS = $(CFLAGS_SIGFOX)
        $(BUILD)/sigfox/timer.o: CFLAGS = $(CFLAGS_SIGFOX)
        $(BUILD)/sigfox/transmission.o: CFLAGS = $(CFLAGS_SIGFOX)
        $(BUILD)/sigfox/targets/%.o: CFLAGS = $(CFLAGS_SIGFOX)
        $(BUILD)/lora/spi-board.o: CFLAGS = $(CFLAGS_SIGFOX)
    endif
endif
ifeq ($(BOARD), SIPY)
    APP_BIN = $(BUILD)/sipy.bin
    ifeq ($(MOD_SIGFOX_ENABLED), 1)
        $(BUILD)/sigfox/radio.o: CFLAGS = $(CFLAGS_SIGFOX)
        $(BUILD)/sigfox/timer.o: CFLAGS = $(CFLAGS_SIGFOX)
        $(BUILD)/sigfox/transmission.o: CFLAGS = $(CFLAGS_SIGFOX)
        $(BUILD)/sigfox/targets/%.o: CFLAGS = $(CFLAGS_SIGFOX)
        $(BUILD)/lora/spi-board.o: CFLAGS = $(CFLAGS_SIGFOX)
    endif
endif
ifeq ($(BOARD), GPY)
    APP_BIN = $(BUILD)/gpy.bin
endif
ifeq ($(BOARD), FIPY)
    APP_BIN = $(BUILD)/fipy.bin
    ifeq ($(MOD_SIGFOX_ENABLED), 1)
        $(BUILD)/sigfox/radio_sx127x.o: CFLAGS = $(CFLAGS_SIGFOX)
        $(BUILD)/sigfox/timer.o: CFLAGS = $(CFLAGS_SIGFOX)
        $(BUILD)/sigfox/transmission.o: CFLAGS = $(CFLAGS_SIGFOX)
        $(BUILD)/sigfox/targets/%.o: CFLAGS = $(CFLAGS_SIGFOX)
        $(BUILD)/lora/spi-board.o: CFLAGS = $(CFLAGS_SIGFOX)
    endif
endif

PART_BIN_8MB = $(BUILD)/lib/partitions_8MB.bin
PART_BIN_4MB = $(BUILD)/lib/partitions_4MB.bin
PART_BIN_ENCRYPT_4MB = $(PART_BIN_4MB)_enc
PART_BIN_ENCRYPT_8MB = $(PART_BIN_8MB)_enc
APP_BIN_ENCRYPT = $(APP_BIN)_enc
APP_IMG  = $(BUILD)/appimg.bin
PART_CSV_8MB = lib/partitions_8MB.csv
PART_CSV_4MB = lib/partitions_4MB.csv
APP_BIN_ENCRYPT_2_8MB = $(APP_BIN)_enc_0x210000
APP_BIN_ENCRYPT_2_4MB = $(APP_BIN)_enc_0x1C0000

ESPPORT ?= /dev/ttyUSB0
ESPBAUD ?= 921600

FLASH_SIZE = detect
ESPFLASHFREQ = 80m
ESPFLASHMODE = dio

PIC_TOOL = $(PYTHON) tools/pypic.py --port $(ESPPORT)
ENTER_FLASHING_MODE = $(PIC_TOOL) --enter
EXIT_FLASHING_MODE = $(PIC_TOOL) --exit

ESP_UPDATER_PY = $(PYTHON) ./tools/fw_updater/updater.py
ESPTOOLPY = $(PYTHON) $(IDF_PATH)/components/esptool_py/esptool/esptool.py --chip esp32
ESPRESET ?= --before default_reset --after no_reset
ESPTOOLPY_SERIAL = $(ESPTOOLPY) --port $(ESPPORT) --baud $(ESPBAUD) $(ESPRESET)
ESP_UPDATER_PY_SERIAL = $(ESP_UPDATER_PY) --port $(ESPPORT) --speed $(ESPBAUD)
BOARD_L = `echo $(BOARD) | tr '[IOY]' '[ioy]'`
SW_VERSION = `cat pycom_version.h |grep SW_VERSION_NUMBER | cut -d'"' -f2`
ESPTOOLPY_WRITE_FLASH  = $(ESPTOOLPY_SERIAL) write_flash -z --flash_mode $(ESPFLASHMODE) --flash_freq $(ESPFLASHFREQ) --flash_size $(FLASH_SIZE)
ESPTOOLPY_ERASE_FLASH  = $(ESPTOOLPY_SERIAL) erase_flash

ESP_UPDATER_PY_WRITE_FLASH  = $(ESP_UPDATER_PY_SERIAL) flash
ESP_UPDATER_PY_ERASE_FLASH  = $(ESP_UPDATER_PY_SERIAL) erase_all
ESP_UPDATER_ALL_FLASH_ARGS = -t $(BUILD_DIR)/$(BOARD_L)-$(SW_VERSION).tar.gz
ESP_UPDATER_ALL_FLASH_ARGS_ENC = -t $(BUILD_DIR)/$(BOARD_L)-$(SW_VERSION)_ENC.tar.gz --secureboot

ESPSECUREPY = $(PYTHON) $(IDF_PATH)/components/esptool_py/esptool/espsecure.py
ESPEFUSE = $(PYTHON) $(IDF_PATH)/components/esptool_py/esptool/espefuse.py --port $(ESPPORT)

# actual command for signing a binary
SIGN_BINARY = $(ESPSECUREPY) sign_data --keyfile $(SECURE_KEY)

# actual command for signing a binary
# it should be used as:
# $(ENCRYPT_BINARY) $(ENCRYPT_0x10000) -o image_encrypt.bin image.bin
ENCRYPT_BINARY = $(ESPSECUREPY) encrypt_flash_data --keyfile $(ENCRYPT_KEY)
ENCRYPT_0x10000 = --address 0x10000
ENCRYPT_APP_PART_2_8MB = --address 0x210000
ENCRYPT_APP_PART_2_4MB = --address 0x1C0000

GEN_ESP32PART := $(PYTHON) $(ESP_IDF_COMP_PATH)/partition_table/gen_esp32part.py -q

ifeq ($(TARGET), app)
all: $(APP_BIN)
endif
ifeq ($(TARGET), boot)
all: $(BOOT_BIN)
endif
ifeq ($(TARGET), boot_app)
all: $(BOOT_BIN) $(APP_BIN)
endif
ifeq ($(TARGET), sigfox)
include sigfox.mk
endif
.PHONY: all CHECK_DEP

$(info Variant: $(VARIANT))
ifeq ($(SECURE), on)

# add #define CONFIG_FLASH_ENCRYPTION_ENABLE 1 used for Flash Encryption
# it can also be added permanently in sdkconfig.h
CFLAGS += -DCONFIG_FLASH_ENCRYPTION_ENABLED=1

# add #define CONFIG_SECURE_BOOT_ENABLED 1 used for Secure Boot
# it can also be added permanently in sdkconfig.h
CFLAGS += -DCONFIG_SECURE_BOOT_ENABLED=1

define resolvepath
$(abspath $(foreach dir,$(1),$(if $(filter /%,$(dir)),$(dir),$(subst //,/,$(2)/$(dir)))))
endef

define dequote
$(subst ",,$(1))
endef

# find the configured private key file
ORIG_SECURE_KEY := $(call resolvepath,$(call dequote,$(SECURE_KEY)),$(PROJECT_PATH))

$(ORIG_SECURE_KEY):
	$(ECHO) "Secure boot signing key '$@' missing. It can be generated using: "
	$(ECHO) "$(ESPSECUREPY) generate_signing_key $(SECURE_KEY)"
	exit 1

# public key name; the name is important
# because it will go into the elf with symbols having name derived out of this one
SECURE_BOOT_VERIFICATION_KEY = signature_verification_key.bin

# verification key derived from signing key.
$(SECURE_BOOT_VERIFICATION_KEY): $(ORIG_SECURE_KEY)
	$(ESPSECUREPY) extract_public_key --keyfile $< $@

# key used for bootloader digest
SECURE_BOOTLOADER_KEY = secure-bootloader-key.bin

$(SECURE_BOOTLOADER_KEY): $(ORIG_SECURE_KEY)
	$(ESPSECUREPY) digest_private_key --keyfile $< $@

# the actual digest+bootloader, that needs to be flashed at address 0x0
BOOTLOADER_REFLASH_DIGEST = 	$(BUILD)/bootloader/bootloader-reflash-digest.bin
BOOTLOADER_REFLASH_DIGEST_ENC = $(BOOTLOADER_REFLASH_DIGEST)_enc

ORIG_ENCRYPT_KEY := $(call resolvepath,$(call dequote,$(ENCRYPT_KEY)),$(PROJECT_PATH))
$(ORIG_ENCRYPT_KEY):
	$(ECHO) "WARNING: Encryption key '$@' missing. It can be created using: "
	$(ECHO) "$(ESPSECUREPY) generate_flash_encryption_key $(ENCRYPT_KEY)"
	exit 1

else #ifeq ($(SECURE), on)
SECURE_BOOT_VERIFICATION_KEY =
SECURE_BOOTLOADER_KEY =
ORIG_ENCRYPT_KEY =
endif #ifeq ($(SECURE), on)


ifeq ($(TARGET), $(filter $(TARGET), boot boot_app))
$(BUILD)/bootloader/bootloader.a: $(BOOT_OBJ) sdkconfig.h
	$(ECHO) "AR $@"
	$(Q) rm -f $@
	$(Q) $(AR) cru $@ $^

$(BUILD)/bootloader/bootloader.elf: $(BUILD)/bootloader/bootloader.a $(SECURE_BOOT_VERIFICATION_KEY)
ifeq ($(SECURE), on)
# unpack libbootloader_support.a, and archive again using the right key for verifying signatures
	$(ECHO) "Inserting verification key $(SECURE_BOOT_VERIFICATION_KEY) in $@"
	$(Q) $(RM) -f ./bootloader/lib/bootloader_support_temp
	$(Q) $(MKDIR)  ./bootloader/lib/bootloader_support_temp
	$(Q) $(CP) ./bootloader/lib/libbootloader_support.a ./bootloader/lib/bootloader_support_temp/
	$(Q) cd bootloader/lib/bootloader_support_temp/ ; pwd ;\
	$(AR) x libbootloader_support.a ;\
	$(RM) -f $(SECURE_BOOT_VERIFICATION_KEY).bin.o ;\
	$(CP) ../../../$(SECURE_BOOT_VERIFICATION_KEY) . ;\
	$(RM) -f $(SECURE_BOOT_VERIFICATION_KEY).bin.o  libbootloader_support.a ;\
	$(OBJCOPY) $(OBJCOPY_EMBED_ARGS) $(SECURE_BOOT_VERIFICATION_KEY) $(SECURE_BOOT_VERIFICATION_KEY).bin.o ;\
	$(AR) cru libbootloader_support.a *.o ;\
	$(CP) libbootloader_support.a ../
	$(Q) $(RM) -rf ./bootloader/lib/bootloader_support_temp
endif #ifeq ($(SECURE), on)
	$(ECHO) "LINK $(CC) *** $(BOOT_LDFLAGS) *** $(BOOT_LIBS) -o $@"
	$(Q) $(CC) $(BOOT_LDFLAGS) $(BOOT_LIBS) -o $@
	$(Q) $(SIZE) $@

$(BOOT_BIN): $(BUILD)/bootloader/bootloader.elf $(SECURE_BOOTLOADER_KEY) $(ORIG_ENCRYPT_KEY)
	$(ECHO) "IMAGE $@"
	$(Q) $(ESPTOOLPY) elf2image --flash_mode $(ESPFLASHMODE) --flash_freq $(ESPFLASHFREQ) -o $@ $<
ifeq ($(SECURE), on)
	# obtain the bootloader digest
	$(Q) $(ESPSECUREPY) digest_secure_bootloader -k $(SECURE_BOOTLOADER_KEY)  -o $(BOOTLOADER_REFLASH_DIGEST) $@
	$(ECHO) "Encrypt Bootloader digest (for offset 0x0)"
	$(Q) $(ENCRYPT_BINARY) --address 0x0 -o $(BOOTLOADER_REFLASH_DIGEST_ENC) $(BOOTLOADER_REFLASH_DIGEST)
	$(RM) -f $(BOOTLOADER_REFLASH_DIGEST)
	$(CP) -f $(BOOTLOADER_REFLASH_DIGEST_ENC) $(BOOT_BIN)
	$(ECHO) $(SEPARATOR)
	$(ECHO) $(SEPARATOR)
	$(ECHO) "Steps for using Secure Boot and Flash Encryption:"
	$(ECHO) $(SEPARATOR)
	$(ECHO) "* Prerequisites: hold valid keys for Flash Encryption and Secure Boot"
	$(ECHO) "$(ESPSECUREPY) generate_flash_encryption_key $(ENCRYPT_KEY)"
	$(ECHO) "$(ESPSECUREPY) generate_signing_key $(SECURE_KEY)"
	$(ECHO) $(SEPARATOR)
	$(ECHO) "* Flash keys: write encryption and secure boot EFUSEs (Irreversible operation)"
	$(ECHO) "$(ESPEFUSE) burn_key flash_encryption $(ENCRYPT_KEY)"
	$(ECHO) "$(ESPEFUSE) burn_key secure_boot $(SECURE_BOOTLOADER_KEY)"
	$(ECHO) "$(ESPEFUSE) burn_efuse FLASH_CRYPT_CNT"
	$(ECHO) "$(ESPEFUSE) burn_efuse FLASH_CRYPT_CONFIG 0x0F"
	$(ECHO) "$(ESPEFUSE) burn_efuse ABS_DONE_0"
	$(ECHO) $(SEPARATOR)
	$(ECHO) "* Flash: write bootloader_digest + partition + app all encrypted"
	$(ECHO) "Hint: 'make BOARD=$(BOARD) SECURE=on flash' can be used"
ifeq ($(BOARD), $(filter $(BOARD), FIPY GPY LOPY4))
	$(ECHO) "$(ESPTOOLPY_WRITE_FLASH) 0x0 $(BOOTLOADER_REFLASH_DIGEST_ENC) $(PART_OFFSET) $(PART_BIN_ENCRYPT_8MB) $(APP_OFFSET) $(APP_BIN_ENCRYPT)"
else
ifeq ($(BOARD), $(filter $(BOARD), SIPY))
	$(ECHO) "$(ESPTOOLPY_WRITE_FLASH) 0x0 $(BOOTLOADER_REFLASH_DIGEST_ENC) $(PART_OFFSET) $(PART_BIN_ENCRYPT_8MB) $(APP_OFFSET) $(APP_BIN_ENCRYPT)"
	$(ECHO) "Generating Encrypted Images for 4MB devices, you can use make flash and it would be handled automatically!"
endif #($(BOARD), $(filter $(BOARD), SIPY))
	$(ECHO) "$(ESPTOOLPY_WRITE_FLASH) 0x0 $(BOOTLOADER_REFLASH_DIGEST_ENC) $(PART_OFFSET) $(PART_BIN_ENCRYPT_4MB) $(APP_OFFSET) $(APP_BIN_ENCRYPT)"
endif #ifeq ($(BOARD), $(filter $(BOARD), FIPY GPY LOPY4))
	$(ECHO) $(SEPARATOR)
	$(ECHO) $(SEPARATOR)
endif #ifeq ($(SECURE), on)
endif #ifeq ($(TARGET), $(filter $(TARGET), boot boot_app))


ifeq ($(TARGET), $(filter $(TARGET), app boot_app))

$(BUILD)/application.a: $(OBJ)
	$(ECHO) "AR $@"
	$(Q) rm -f $@
	$(Q) $(AR) cru $@ $^
$(BUILD)/application.elf: $(BUILD)/application.a $(BUILD)/esp32_out.ld esp32.project.ld $(SECURE_BOOT_VERIFICATION_KEY)
ifeq ($(SECURE), on)
# unpack libbootloader_support.a, and archive again using the right key for verifying signatures
	$(ECHO) "Inserting verification key $(SECURE_BOOT_VERIFICATION_KEY) in $@"
	$(Q) $(RM) -rf ./lib/bootloader_support_temp
	$(Q) $(MKDIR)  ./lib/bootloader_support_temp
	$(Q) $(CP) ./lib/libbootloader_support.a ./lib/bootloader_support_temp/
	$(Q) cd lib/bootloader_support_temp/ ; pwd ;\
	$(AR) x libbootloader_support.a ;\
	$(RM) -f $(SECURE_BOOT_VERIFICATION_KEY).bin.o ;\
	$(CP) ../../$(SECURE_BOOT_VERIFICATION_KEY) . ;\
	$(RM) -f $(SECURE_BOOT_VERIFICATION_KEY).bin.o  libbootloader_support.a ;\
	$(OBJCOPY) $(OBJCOPY_EMBED_ARGS) $(SECURE_BOOT_VERIFICATION_KEY) $(SECURE_BOOT_VERIFICATION_KEY).bin.o ;\
	$(AR) cru libbootloader_support.a *.o ;\
	$(CP) libbootloader_support.a ../
	$(Q) $(RM) -rf lib/bootloader_support_temp
endif #ifeq ($(SECURE), on)
	$(ECHO) "LINK $@"
	$(Q) $(CC) $(APP_LDFLAGS) $(APP_LIBS) -o $@
	$(SIZE) $@

$(APP_BIN): $(BUILD)/application.elf $(PART_BIN_4MB) $(PART_BIN_8MB) $(ORIG_ENCRYPT_KEY)
	$(ECHO) "IMAGE $@"
	$(Q) $(ESPTOOLPY) elf2image --flash_mode $(ESPFLASHMODE) --flash_freq $(ESPFLASHFREQ) -o $@ $<
ifeq ($(SECURE), on)
	$(ECHO) "Signing $@"
	$(Q) $(SIGN_BINARY) $@
	$(ECHO) $(SEPARATOR)
ifeq ($(BOARD), $(filter $(BOARD), FIPY GPY LOPY4))
	$(ECHO) "Encrypt image into $(APP_BIN_ENCRYPT) (0x10000 offset) and $(APP_BIN_ENCRYPT_2_8MB) (0x210000 offset)"
else
ifneq ($(BOARD), $(filter $(BOARD), SIPY))
	$(ECHO) "Encrypt image into $(APP_BIN_ENCRYPT) (0x10000 offset) and $(APP_BIN_ENCRYPT_2_8MB) (0x210000 offset)"
	$(ECHO) "And"
endif
	$(ECHO) "Encrypt image into $(APP_BIN_ENCRYPT) (0x10000 offset) and $(APP_BIN_ENCRYPT_2_4MB) (0x1C0000 offset)"
endif
	$(Q) $(ENCRYPT_BINARY) $(ENCRYPT_0x10000) -o $(APP_BIN_ENCRYPT) $@
ifeq ($(BOARD), $(filter $(BOARD), FIPY GPY LOPY4))
	$(Q) $(ENCRYPT_BINARY) $(ENCRYPT_APP_PART_2_8MB) -o $(APP_BIN_ENCRYPT_2_8MB) $@
else
ifneq ($(BOARD), $(filter $(BOARD), SIPY))
	$(Q) $(ENCRYPT_BINARY) $(ENCRYPT_APP_PART_2_8MB) -o $(APP_BIN_ENCRYPT_2_8MB) $@
endif
	$(Q) $(ENCRYPT_BINARY) $(ENCRYPT_APP_PART_2_4MB) -o $(APP_BIN_ENCRYPT_2_4MB) $@
endif
	$(ECHO) "Overwrite $(APP_BIN) with $(APP_BIN_ENCRYPT)"
	$(CP) -f $(APP_BIN_ENCRYPT) $(APP_BIN)
	$(ECHO) $(SEPARATOR)
	$(ECHO) $(SEPARATOR)
	$(ECHO) "Steps for using Secure Boot and Flash Encryption:"
	$(ECHO) $(SEPARATOR)
	$(ECHO) "* Prerequisites: hold valid keys for Flash Encryption and Secure Boot"
	$(ECHO) "$(ESPSECUREPY) generate_flash_encryption_key $(ENCRYPT_KEY)"
	$(ECHO) "$(ESPSECUREPY) generate_signing_key $(SECURE_KEY)"
	$(ECHO) $(SEPARATOR)
	$(ECHO) "* Flash keys: write encryption and secure boot EFUSEs (Irreversible operation)"
	$(ECHO) "$(ESPEFUSE) burn_key flash_encryption $(ENCRYPT_KEY)"
	$(ECHO) "$(ESPEFUSE) burn_key secure_boot $(SECURE_BOOTLOADER_KEY)"
	$(ECHO) "$(ESPEFUSE) burn_efuse FLASH_CRYPT_CNT"
	$(ECHO) "$(ESPEFUSE) burn_efuse FLASH_CRYPT_CONFIG 0x0F"
	$(ECHO) "$(ESPEFUSE) burn_efuse ABS_DONE_0"
	$(ECHO) $(SEPARATOR)
	$(ECHO) "* Flash: write bootloader_digest + partition + app all encrypted"
	$(ECHO) "Hint: 'make BOARD=$(BOARD) SECURE=on flash' can be used"
ifeq ($(BOARD), $(filter $(BOARD), FIPY GPY LOPY4))
	$(ECHO) "$(ESPTOOLPY_WRITE_FLASH) 0x0 $(BOOTLOADER_REFLASH_DIGEST_ENC) $(PART_OFFSET) $(PART_BIN_ENCRYPT_8MB) $(APP_OFFSET) $(APP_BIN_ENCRYPT)"
else
ifeq ($(BOARD), $(filter $(BOARD), SIPY))
	$(ECHO) "$(ESPTOOLPY_WRITE_FLASH) 0x0 $(BOOTLOADER_REFLASH_DIGEST_ENC) $(PART_OFFSET) $(PART_BIN_ENCRYPT_8MB) $(APP_OFFSET) $(APP_BIN_ENCRYPT)"
	$(ECHO) "Generating Encrypted Images for 4MB devices, you can use make flash and it would be handled automatically!"
endif #($(BOARD), $(filter $(BOARD), SIPY))
	$(ECHO) "$(ESPTOOLPY_WRITE_FLASH) 0x0 $(BOOTLOADER_REFLASH_DIGEST_ENC) $(PART_OFFSET) $(PART_BIN_ENCRYPT_4MB) $(APP_OFFSET) $(APP_BIN_ENCRYPT)"
endif #ifeq ($(BOARD), $(filter $(BOARD), FIPY GPY LOPY4))
	$(ECHO) $(SEPARATOR)
	$(ECHO) $(SEPARATOR)
endif # feq ($(SECURE), on)

$(BUILD)/esp32_out.ld: $(ESP_IDF_COMP_PATH)/esp32/ld/esp32.ld sdkconfig.h
	$(ECHO) "CPP $@"
	$(Q) $(CC) -I. -C -P -x c -E $< -o $@
endif #ifeq ($(TARGET), $(filter $(TARGET), app boot_app))

release: $(APP_BIN) $(BOOT_BIN)
	$(ECHO) "checking size of image"
	$(Q) bash tools/size_check.sh $(BOARD) $(BTYPE) $(VARIANT)
ifeq ($(SECURE), on)
	$(Q) tools/makepkg.sh $(BOARD) $(RELEASE_DIR) $(BUILD) 1
else
	$(Q) tools/makepkg.sh $(BOARD) $(RELEASE_DIR) $(BUILD)
endif

flash: release
	$(ECHO) "checking size of image"
	$(Q) bash tools/size_check.sh $(BOARD) $(BTYPE) $(VARIANT)

	$(ECHO) "Flashing project"
ifeq ($(SECURE), on)
	$(ECHO) $(SEPARATOR)
	$(ECHO) "(Secure boot enabled, so bootloader + digest is flashed)"
	$(ECHO) $(SEPARATOR)
	$(ECHO) "$(Q) $(ESP_UPDATER_PY_WRITE_FLASH) $(ESP_UPDATER_ALL_FLASH_ARGS_ENC)"
	$(Q) $(ESP_UPDATER_PY_WRITE_FLASH) $(ESP_UPDATER_ALL_FLASH_ARGS_ENC)
else # ifeq ($(SECURE), on)
	$(ECHO) "$(ESP_UPDATER_PY_WRITE_FLASH) $(ESP_UPDATER_ALL_FLASH_ARGS)"
	$(Q) $(ESP_UPDATER_PY_WRITE_FLASH) $(ESP_UPDATER_ALL_FLASH_ARGS)
endif

erase:
	$(ECHO) "Erasing flash"
	$(Q) $(ESP_UPDATER_PY_ERASE_FLASH)

$(PART_BIN_4MB): $(PART_CSV_4MB) $(ORIG_ENCRYPT_KEY)
	$(ECHO) "Building partitions from $(PART_CSV_4MB)..."
	$(Q) $(GEN_ESP32PART) $< $@
ifeq ($(SECURE), on)
	$(ECHO) "Signing $@"
	$(Q) $(SIGN_BINARY) $@
	$(ECHO) "Encrypt paritions table image into $(PART_BIN_ENCRYPT_4MB) (by default 0x8000 offset)"
	$(Q) $(ENCRYPT_BINARY) --address 0x8000 -o $(PART_BIN_ENCRYPT_4MB) $@
endif # ifeq ($(SECURE), on)
$(PART_BIN_8MB): $(PART_CSV_8MB) $(ORIG_ENCRYPT_KEY)
	$(ECHO) "Building partitions from $(PART_CSV_8MB)..."
	$(Q) $(GEN_ESP32PART) $< $@
ifeq ($(SECURE), on)
	$(ECHO) "Signing $@"
	$(Q) $(SIGN_BINARY) $@
	$(ECHO) "Encrypt paritions table image into $(PART_BIN_ENCRYPT_8MB) (by default 0x8000 offset)"
	$(Q) $(ENCRYPT_BINARY) --address 0x8000 -o $(PART_BIN_ENCRYPT_8MB) $@
endif # ifeq ($(SECURE), on)

show_partitions: $(PART_BIN_4MB) $(PART_BIN_8MB)
	$(ECHO) "Partition table 4MB binary generated. Contents:"
	$(ECHO) $(SEPARATOR)
	$(Q) $(GEN_ESP32PART) $<
	$(ECHO) $(SEPARATOR)
	$(ECHO) "Partition table 8MB binary generated. Contents:"
	$(ECHO) $(SEPARATOR)
	$(Q) $(GEN_ESP32PART) $(word 2,$^)
	$(ECHO) $(SEPARATOR)

flash_mode:
	$(ENTER_FLASHING_MODE)

MAKE_PINS = boards/make-pins.py
BOARD_PINS = boards/$(BOARD)/pins.csv
AF_FILE = boards/esp32_af.csv
PREFIX_FILE = boards/esp32_prefix.c
GEN_PINS_SRC = $(BUILD)/pins.c
GEN_PINS_HDR = $(HEADER_BUILD)/pins.h
GEN_PINS_QSTR = $(BUILD)/pins_qstr.h

.NOTPARALLEL: CHECK_DEP $(OBJ)
.NOTPARALLEL: CHECK_DEP $(BOOT_OBJ)

$(BOOT_OBJ) $(OBJ): | CHECK_DEP

# Making OBJ use an order-only dependency on the generated pins.h file
# has the side effect of making the pins.h file before we actually compile
# any of the objects. The normal dependency generation will deal with the
# case when pins.h is modified. But when it doesn't exist, we don't know
# which source files might need it.
$(OBJ): | $(GEN_PINS_HDR)

# Check Dependencies (IDF version, Frozen code and IDF LIBS)
CHECK_DEP:
	$(Q) bash tools/idfVerCheck.sh $(IDF_PATH) "$(IDF_HASH)"
	$(Q) bash tools/mpy-build-check.sh $(BOARD) $(BTYPE) $(VARIANT)
	$(Q) $(PYTHON) check_secure_boot.py --SECURE $(SECURE)
ifeq ($(COPY_IDF_LIB), 1)
	$(ECHO) "COPY IDF LIBRARIES"
	$(Q) $(PYTHON) get_idf_libs.py --idflibs $(IDF_PATH)/examples/wifi/scan/build
endif

# Call make-pins.py to generate both pins_gen.c and pins.h
$(GEN_PINS_SRC) $(GEN_PINS_HDR) $(GEN_PINS_QSTR): $(BOARD_PINS) $(MAKE_PINS) $(AF_FILE) $(PREFIX_FILE) | $(HEADER_BUILD)
	$(ECHO) "Create $@"
	$(Q)$(PYTHON) $(MAKE_PINS) --board $(BOARD_PINS) --af $(AF_FILE) --prefix $(PREFIX_FILE) --hdr $(GEN_PINS_HDR) --qstr $(GEN_PINS_QSTR) > $(GEN_PINS_SRC)

$(BUILD)/pins.o: $(BUILD)/pins.c
	$(call compile_c)
