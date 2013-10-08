### Application-specific constants

APP_NAME=loragw_pkt_logger

### constant symbols

CC=gcc
CFLAGS=-O2 -Iinc
C99FLAGS=-O2 -std=c99 -Iinc

### constants for Lora Gateway HAL library

LGW_PATH=../loragw_hal
LGW_INC=-I$(LGW_PATH)/inc
#LGW_LNK=-lloragw -lrt
LGW_LNK=-lloragw -lrt -lmpsse
# add libmpsse or not, depending on what option you compiled the libloragw with

### general build targets

all: $(APP_NAME)

clean:
	rm -f obj/*.o
	rm -f $(APP_NAME)

### sub-modules compilation

### main program compilation and assembly

obj/$(APP_NAME).o: src/$(APP_NAME).c 
	$(CC) -c $(C99FLAGS) $(LGW_INC) src/$(APP_NAME).c -o obj/$(APP_NAME).o $(FLAG_AUX)

$(APP_NAME): $(LGW_PATH)/libloragw.a obj/$(APP_NAME).o
	$(CC) -o $(APP_NAME) obj/$(APP_NAME).o -L$(LGW_PATH) $(LGW_LNK)
