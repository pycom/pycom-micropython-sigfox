### Application-specific constants

APP_NAME=loragw_band_survey

### get external defined data

include ../target.cfg

### constant symbols

CROSS_COMPILE=
CC=$(CROSS_COMPILE)gcc
AR=$(CROSS_COMPILE)ar

CFLAGS=-O2 -Wall -Wextra -std=c99 -Iinc -I.

### constants for Lora Gateway HAL library

LGW_PATH=../libloragw
LGW_INC=-I$(LGW_PATH)/inc

### configuration file processing

ifeq ($(CFG_RADIO),sx1257)
  CFGFLAGS= -DCFG_RAD_1257
else ifeq ($(CFG_RADIO),sx1255)
  CFGFLAGS= -DCFG_RAD_1255
else
  $(error No radio chip(s) selected, check ../target.cfg file)
endif

### linking options

ifeq ($(CFG_SPIPHY),native)
  LIBS=-lloragw -lrt
else ifeq ($(CFG_SPIPHY),ftdi)
  LIBS=-lloragw -lrt -lmpsse
endif

### general build targets

all: $(APP_NAME)

clean:
	rm -f obj/*.o
	rm -f $(APP_NAME)

.cfg_ok: ../target.cfg
	@touch .cfg_ok

### main program compilation and assembly

obj/$(APP_NAME).o: src/$(APP_NAME).c src/rssi_fw.var
	$(CC) -c $(CFLAGS) $(LGW_INC) $(CFGFLAGS) src/$(APP_NAME).c -o $@

$(APP_NAME): obj/$(APP_NAME).o $(LGW_PATH)/libloragw.a .cfg_ok
	$(CC) -L$(LGW_PATH) $< -o $@ $(LIBS)

### EOF