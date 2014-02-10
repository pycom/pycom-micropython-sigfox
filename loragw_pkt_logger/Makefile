### Application-specific constants

APP_NAME=loragw_pkt_logger

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

ifeq ($(CFG_SPIPHY),native)
  LIBS=-lloragw -lrt
else ifeq ($(CFG_SPIPHY),ftdi)
  LIBS=-lloragw -lrt -lmpsse
endif

### general build targets

all: $(APP_NAME) global_conf.json

clean:
	rm -f obj/*.o
	rm -f $(APP_NAME)
	-unlink global_conf.json 2>/dev/null

.cfg_ok: ../target.cfg
	@touch .cfg_ok

### sub-modules compilation

obj/parson.o: src/parson.c inc/parson.h
	$(CC) -c $(CFLAGS) $(LGW_INC) $< -o $@

### select the proper configuration JSON for the program

ifeq ($(CFG_BAND),eu868)
global_conf.json: cfg/global_conf.eu868.json
	ln -s $< $@
else ifeq ($(CFG_BAND),us915)
global_conf.json: cfg/global_conf.us915.json
	ln -s $< $@
else ifeq ($(CFG_BAND),cn470)
global_conf.json: cfg/global_conf.cn470.json
	ln -s $< $@
else
global_conf.json: cfg/global_conf.empty.json
	ln -s $< $@
endif

### main program compilation and assembly

obj/$(APP_NAME).o: src/$(APP_NAME).c inc/parson.h
	$(CC) -c $(CFLAGS) $(LGW_INC) $< -o $@

$(APP_NAME): obj/$(APP_NAME).o $(LGW_PATH)/libloragw.a obj/parson.o .cfg_ok
	$(CC) -L$(LGW_PATH) $< obj/parson.o -o $@ $(LIBS)

### EOF