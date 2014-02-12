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

OPT_CFG= -DRELEASE_VERSION="\"$(RELEASE_VERSION)\""

ifeq ($(CFG_SPI),native)
  OPT_CFG += -DCFG_SPI_NATIVE
  CFG_SPI_MSG= Linux native SPI driver (/dev/spidev0.0)
else ifeq ($(CFG_SPI),ftdi)
  OPT_CFG += -DCFG_SPI_FTDI
  CFG_SPI_MSG= FTDI SPI-over-USB bridge using libmpsse/libftdi/libusb
else
  $(error No SPI physical layer selected, check ../target.cfg file)
endif

ifeq ($(CFG_CHIP),sx1301)
  OPT_CFG += -DCFG_CHIP_1301
  CFG_CHIP_MSG= Semtech SX1301 production chip
else ifeq ($(CFG_CHIP),fpga1301)
  OPT_CFG += -DCFG_CHIP_FPGA
  CFG_CHIP_MSG= FPGA containing the SX1301 IP
else
  $(error No concentrator chip selected, check ../target.cfg file)
endif

ifeq ($(CFG_RADIO),sx1257)
  OPT_CFG += -DCFG_RADIO_1257
  CFG_RADIO_MSG= Dual SX1257 transceivers, covering 860-1000 MHz
else ifeq ($(CFG_RADIO),sx1255)
  OPT_CFG += -DCFG_RADIO_1255
  CFG_RADIO_MSG= Dual SX1255 transceivers, covering 400-510 MHz
else
  $(error No radio chip(s) selected, check ../target.cfg file)
endif

ifeq ($(CFG_BAND),full)
  OPT_CFG += -DCFG_BAND_FULL
  CFG_BAND_MSG= Full range supported by the radio(s)
else ifeq ($(CFG_BAND),eu868)
  OPT_CFG += -DCFG_BAND_868
  CFG_BAND_MSG= ETSI 868 (866) MHz band
else ifeq ($(CFG_BAND),us915)
  OPT_CFG += -DCFG_BAND_915
  CFG_BAND_MSG= FCC 915 MHz band
else ifeq ($(CFG_BAND),cn470)
  OPT_CFG += -DCFG_BAND_470
  CFG_BAND_MSG= China 470 MHz band
#else ifeq ($(CFG_BAND),jp920)
#  OPT_CFG += -DCFG_BAND_920
#  CFG_BAND_MSG= Japan 920 MHz band
#else ifeq ($(CFG_BAND),eu433)
#  OPT_CFG += -DCFG_BAND_433
#  CFG_BAND_MSG= ETSI 433 MHz band
else
  $(warning No frequency band selected, full radio capability assumed)
  OPT_CFG += -DCFG_BAND_FULL
  CFG_BAND_MSG= Full range supported by the radio(s)
endif

ifeq ($(CFG_CAL),dev_nano_868)
  OPT_CFG += -DCFG_CAL_NANO868
  CFG_CAL_MSG= FPGA-based nano-concentrator, with 868 MHz SAW filter
else ifeq ($(CFG_CAL),ref_1301_57nf)
  OPT_CFG += -DCFG_CAL_REF1301
  CFG_CAL_MSG= SX1301 reference board with SX1257 radios, no filters
else
  $(warning No calibration selected, check ../target.cfg file)
  OPT_CFG += -DCFG_CAL_NONE
  CFG_CAL_MSG= None
endif

### propagate some constants to C code

OPT_AUX= -DDEBUG_AUX=$(DEBUG_AUX)
OPT_SPI= -DDEBUG_SPI=$(DEBUG_SPI)
OPT_REG= -DDEBUG_REG=$(DEBUG_REG)
OPT_GPS= -DDEBUG_GPS=$(DEBUG_GPS)
OPT_HAL= -DDEBUG_HAL=$(DEBUG_HAL)

### linking options

ifeq ($(CFG_SPI),native)
  LIBS=-lrt
else ifeq ($(CFG_SPI),ftdi)
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
	@echo "Radio chip(s)    : $(CFG_RADIO_MSG)"
	@echo "Frequency band   : $(CFG_BAND_MSG)"
	@echo "Calibration param: $(CFG_CAL_MSG)"
	@echo "*** Configuration seems ok ***"
	@echo ""
	@touch .cfg_ok

### library module target

obj/loragw_aux.o: src/loragw_aux.c inc/loragw_aux.h debug.cfg
	$(CC) -c $(CFLAGS) $(OPT_AUX) $< -o $@

ifeq ($(CFG_SPI),native)
obj/loragw_spi.o: src/loragw_spi.native.c inc/loragw_spi.h debug.cfg .cfg_ok
	$(CC) -c $(CFLAGS) $(OPT_SPI) $< -o $@
else ifeq ($(CFG_SPI),ftdi)
obj/loragw_spi.o: src/loragw_spi.ftdi.c inc/loragw_spi.h debug.cfg .cfg_ok
	$(CC) -c $(CFLAGS) $(OPT_SPI) $< -o $@
endif

obj/loragw_reg.o: src/loragw_reg.c inc/loragw_reg.h inc/loragw_spi.h debug.cfg
	$(CC) -c $(CFLAGS) $(OPT_REG) $< -o $@

obj/loragw_hal.o: src/loragw_hal.c inc/loragw_hal.h inc/loragw_reg.h inc/loragw_aux.h src/arb_fw.var src/agc_fw.var debug.cfg .cfg_ok
	$(CC) -c $(CFLAGS) $(OPT_HAL) $(OPT_CFG) $< -o $@

obj/loragw_gps.o: src/loragw_gps.c inc/loragw_gps.h debug.cfg
	$(CC) -c $(CFLAGS) $(OPT_GPS) $< -o $@

### static library

libloragw.a: obj/loragw_hal.o obj/loragw_gps.o obj/loragw_reg.o obj/loragw_spi.o obj/loragw_aux.o
	$(AR) rcs $@ $^

### test programs

test_loragw_spi: tst/test_loragw_spi.c libloragw.a .cfg_ok
	$(CC) $(CFLAGS) $< libloragw.a -o $@ $(LIBS)

test_loragw_reg: tst/test_loragw_reg.c libloragw.a .cfg_ok
	$(CC) $(CFLAGS) $< libloragw.a -o $@ $(LIBS)

test_loragw_hal: tst/test_loragw_hal.c libloragw.a .cfg_ok
	$(CC) $(CFLAGS) $(OPT_CFG) $< libloragw.a -o $@ $(LIBS)

test_loragw_gps: tst/test_loragw_gps.c libloragw.a .cfg_ok
	$(CC) $(CFLAGS) $< libloragw.a -o $@ $(LIBS)

### EOF