### Application-specific constants

APP_NAME=loragw_band_survey

### get external defined data

RELEASE_VERSION= `cat ../VERSION`
include ../target.cfg

### constant symbols

CROSS_COMPILE=
CC=$(CROSS_COMPILE)gcc
AR=$(CROSS_COMPILE)ar

CFLAGS=-O2 -Wall -Wextra -std=c99 -Iinc -I.

### constants for Lora Gateway HAL library
# List the library sub-modules that are used by the application

LGW_PATH=../libloragw

LGW_INC=
LGW_INC += $(LGW_PATH)/inc/loragw_hal.h
LGW_INC += $(LGW_PATH)/inc/loragw_reg.h
LGW_INC += $(LGW_PATH)/inc/loragw_aux.h

### configuration file processing

OPT_CFG= -DRELEASE_VERSION="\"$(RELEASE_VERSION)\""

ifeq ($(CFG_SPI),native)
  OPT_CFG += -DCFG_SPI_NATIVE
else ifeq ($(CFG_SPI),ftdi)
  OPT_CFG += -DCFG_SPI_FTDI
else
  $(error Invalid settings in ../target.cfg file.)
endif

ifeq ($(CFG_CHIP),sx1301)
  OPT_CFG += -DCFG_CHIP_1301
else ifeq ($(CFG_CHIP),fpga1301)
  OPT_CFG += -DCFG_CHIP_FPGA
else
  $(error Invalid settings in ../target.cfg file.)
endif

ifeq ($(CFG_RADIO),sx1257)
  OPT_CFG += -DCFG_RADIO_1257
else ifeq ($(CFG_RADIO),sx1255)
  OPT_CFG += -DCFG_RADIO_1255
else
  $(error Invalid settings in ../target.cfg file.)
endif

ifeq ($(CFG_BAND),eu868)
  OPT_CFG += -DCFG_BAND_868
else ifeq ($(CFG_BAND),us915)
  OPT_CFG += -DCFG_BAND_915
else ifeq ($(CFG_BAND),cn470)
  OPT_CFG += -DCFG_BAND_470
else
  OPT_CFG += -DCFG_BAND_FULL
endif

ifeq ($(CFG_CAL),dev_nano_868)
  OPT_CFG += -DCFG_CAL_NANO868
else ifeq ($(CFG_CAL),ref_1301_57nf)
  OPT_CFG += -DCFG_CAL_REF1301
else
  OPT_CFG += -DCFG_CAL_NONE
endif

### linking options

ifeq ($(CFG_SPI),native)
  LIBS=-lloragw -lrt
else ifeq ($(CFG_SPI),ftdi)
  LIBS=-lloragw -lrt -lmpsse
endif

### general build targets

all: $(APP_NAME)

clean:
	rm -f obj/*.o
	rm -f $(APP_NAME)

.cfg_ok: ../target.cfg
	@touch .cfg_ok

### HAL library

$(LGW_PATH)/libloragw.a: .cfg_ok $(LGW_INC)
	$(MAKE) all -C $(LGW_PATH)

### main program compilation and assembly

obj/$(APP_NAME).o: src/$(APP_NAME).c $(LGW_INC) src/rssi_fw.var
	$(CC) -c $(CFLAGS) -I$(LGW_PATH)/inc $(OPT_CFG) $< -o $@

$(APP_NAME): obj/$(APP_NAME).o $(LGW_PATH)/libloragw.a .cfg_ok
	$(CC) -L$(LGW_PATH) $< -o $@ $(LIBS)

### EOF