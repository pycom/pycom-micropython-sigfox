### Environment constants 

ARCH ?=
CROSS_COMPILE ?=
export

### general build targets

all:
	$(MAKE) all -e -C libloragw
	$(MAKE) all -e -C util_boot
	$(MAKE) all -e -C util_chip_id
	$(MAKE) all -e -C util_pkt_logger
	$(MAKE) all -e -C util_com_stress
	$(MAKE) all -e -C util_tx_test
	$(MAKE) all -e -C util_tx_continuous

clean:
	$(MAKE) clean -e -C libloragw
	$(MAKE) clean -e -C util_boot
	$(MAKE) clean -e -C util_chip_id
	$(MAKE) clean -e -C util_pkt_logger
	$(MAKE) clean -e -C util_com_stress
	$(MAKE) clean -e -C util_tx_test
	$(MAKE) clean -e -C util_tx_continuous

### EOF
