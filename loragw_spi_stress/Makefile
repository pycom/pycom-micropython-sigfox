### Application-specific constants

APP_NAME=loragw_spi_stress

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

### main program compilation and assembly

obj/$(APP_NAME).o: src/$(APP_NAME).c
	$(CC) -c $(CFLAGS) $(LGW_INC) $< -o $@

$(APP_NAME): obj/$(APP_NAME).o $(LGW_PATH)/libloragw.a .cfg_ok
	$(CC) -L$(LGW_PATH) $< -o $@ $(LIBS)

### EOF