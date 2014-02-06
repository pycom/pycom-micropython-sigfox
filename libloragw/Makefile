### get external defined data

RELEASE_VERSION= `cat ../VERSION`
include ../target.cfg
include debug.cfg

### constant symbols

CROSS_COMPILE=
CC=$(CROSS_COMPILE)gcc
AR=$(CROSS_COMPILE)ar

CFLAGS=-O2 -Wall -Wextra -std=c99 -Iinc -I.

### configuration file processing

CFGFLAGS=

ifeq ($(CFG_SPIPHY),native)
  CFGFLAGS += -DCFG_SPI_NATIVE
  CFG_SPI_MSG= Linux native SPI driver (/dev/spidev0.0)
else ifeq ($(CFG_SPIPHY),ftdi)
  CFGFLAGS += -DCFG_SPI_FTDI
  CFG_SPI_MSG= FTDI SPI-over-USB bridge using libmpsse/libftdi/libusb
else
  $(error No SPI physical layer selected, check ../target.cfg file)
endif

ifeq ($(CFG_CHIP),sx1301)
  CFGFLAGS += -DCFG_CHIP_1301
  CFG_CHIP_MSG= Semtech SX1301 production chip
else ifeq ($(CFG_CHIP),fpga1301)
  CFGFLAGS += -DCFG_CHIP_FPGA
  CFG_CHIP_MSG= FPGA containing the SX1301 IP
else
  $(error No concentrator chip selected, check ../target.cfg file)
endif

ifeq ($(CFG_RADIO),sx1257)
  CFGFLAGS += -DCFG_RAD_1257
  CFG_RAD_MSG= Dual SX1257 transceivers, covering 860-1000 MHz
else ifeq ($(CFG_RADIO),sx1255)
  CFGFLAGS += -DCFG_RAD_1255
  CFG_RAD_MSG= Dual SX1255 transceivers, covering 400-510 MHz
else
  $(error No radio chip(s) selected, check ../target.cfg file)
endif

ifeq ($(CFG_BAND),full)
  CFGFLAGS += -DCFG_BAND_FULL
  CFG_BAND_MSG= Full range supported by the radio(s)
else ifeq ($(CFG_BAND),eu868)
  CFGFLAGS += -DCFG_BAND_868
  CFG_BAND_MSG= ETSI 868 (866) MHz band
else ifeq ($(CFG_BAND),us915)
  CFGFLAGS += -DCFG_BAND_915
  CFG_BAND_MSG= FCC 915 MHz band
else ifeq ($(CFG_BAND),jp920)
  CFGFLAGS += -DCFG_BAND_920
  CFG_BAND_MSG= Japan 920 MHz band
else ifeq ($(CFG_BAND),cn470)
  CFGFLAGS += -DCFG_BAND_470
  CFG_BAND_MSG= China 470 MHz band
else ifeq ($(CFG_BAND),eu433)
  CFGFLAGS += -DCFG_BAND_433
  CFG_BAND_MSG= ETSI 433 MHz band
else
  $(warning No frequency band selected, full radio capability assumed)
  CFGFLAGS += -DCFG_BAND_FULL
  CFG_BAND_MSG= Full range supported by the radio(s)
endif

ifeq ($(CFG_CALIB),dev_nano_868)
  CFGFLAGS += -DCFG_CAL_NANO868
  CFG_CAL_MSG= FPGA-based nano-concentrator, with 868 MHz SAW filter
else ifeq ($(CFG_CALIB),ref_1301_57nf)
  CFGFLAGS += -DCFG_CAL_REF1301
  CFG_CAL_MSG= SX1301 reference board with SX1257 radios, no filters
else
  $(warning No calibration selected, check ../target.cfg file)
  CFGFLAGS += -DCFG_CAL_NONE
  CFG_CAL_MSG= None
endif

### propagate some constants to C code

CONST_AUX= -DDEBUG_AUX=$(DEBUG_AUX)
CONST_SPI= -DDEBUG_SPI=$(DEBUG_SPI)
CONST_REG= -DDEBUG_REG=$(DEBUG_REG)
CONST_GPS= -DDEBUG_GPS=$(DEBUG_GPS)
CONST_HAL= -DDEBUG_HAL=$(DEBUG_HAL) -DRELEASE_VERSION="\"$(RELEASE_VERSION)\"" $(CFGFLAGS)

### linking options

ifeq ($(CFG_SPIPHY),native)
  LIBS=-lrt
else ifeq ($(CFG_SPIPHY),ftdi)
  LIBS=-lrt -lmpsse
endif

### general build targets

all: libloragw.a test_loragw_spi test_loragw_reg test_loragw_hal test_loragw_gps

clean:
	rm -f libloragw.a
	rm -f test_loragw_*
	rm -f obj/*.o
	rm -f .cfg_ok

.cfg_ok: ../VERSION ../target.cfg
	@echo "*** Checking Lora gateway HAL library configuration ***"
	@rm -f .cfg_ok
	@echo "Release version  : $(RELEASE_VERSION)"
	@echo "SPI interface    : $(CFG_SPI_MSG)"
	@echo "Concentrator chip: $(CFG_CHIP_MSG)"
	@echo "Radio chip(s)    : $(CFG_RAD_MSG)"
	@echo "Frequency band   : $(CFG_BAND_MSG)"
	@echo "Calibration param: $(CFG_CAL_MSG)"
	@echo "*** Configuration seems ok ***"
	@echo ""
	@touch .cfg_ok

### library module target

obj/loragw_aux.o: src/loragw_aux.c inc/loragw_aux.h debug.cfg
	$(CC) -c $(CFLAGS) $(CONST_AUX) $< -o $@

ifeq ($(CFG_SPIPHY),native)
obj/loragw_spi.o: src/loragw_spi.native.c inc/loragw_spi.h debug.cfg .cfg_ok
	$(CC) -c $(CFLAGS) $(CONST_SPI) $< -o $@
else ifeq ($(CFG_SPIPHY),ftdi)
obj/loragw_spi.o: src/loragw_spi.ftdi.c inc/loragw_spi.h debug.cfg .cfg_ok
	$(CC) -c $(CFLAGS) $(CONST_SPI) $< -o $@
endif

obj/loragw_reg.o: src/loragw_reg.c inc/loragw_reg.h inc/loragw_spi.h debug.cfg
	$(CC) -c $(CFLAGS) $(CONST_REG) $< -o $@

obj/loragw_hal.o: src/loragw_hal.c inc/loragw_hal.h inc/loragw_reg.h inc/loragw_aux.h src/arb_fw.var src/agc_fw.var debug.cfg .cfg_ok
	$(CC) -c $(CFLAGS) $(CONST_HAL) $< -o $@

obj/loragw_gps.o: src/loragw_gps.c inc/loragw_gps.h debug.cfg
	$(CC) -c $(CFLAGS) $(CONST_GPS) $< -o $@

### static library

libloragw.a: obj/loragw_hal.o obj/loragw_gps.o obj/loragw_reg.o obj/loragw_spi.o obj/loragw_aux.o
	$(AR) rcs $@ $^

### test programs

test_loragw_spi: tst/test_loragw_spi.c obj/loragw_spi.o
	$(CC) $(CFLAGS) $^ -o $@

test_loragw_reg: tst/test_loragw_reg.c obj/loragw_spi.o obj/loragw_reg.o
	$(CC) $(CFLAGS) $^ -o $@

test_loragw_hal: tst/test_loragw_hal.c obj/loragw_spi.o obj/loragw_reg.o obj/loragw_hal.o obj/loragw_aux.o .cfg_ok
	$(CC) $(CFLAGS) $< obj/loragw_spi.o obj/loragw_reg.o obj/loragw_hal.o obj/loragw_aux.o -o $@ $(LIBS)

test_loragw_gps: tst/test_loragw_gps.c obj/loragw_spi.o obj/loragw_reg.o obj/loragw_hal.o obj/loragw_aux.o obj/loragw_gps.o .cfg_ok
	$(CC) $(CFLAGS) $< obj/loragw_spi.o obj/loragw_reg.o obj/loragw_hal.o obj/loragw_aux.o obj/loragw_gps.o -o $@ $(LIBS)

### EOF