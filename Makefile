### general build targets

all:
	$(MAKE) all -C libloragw
	$(MAKE) all -C util_band_survey
	$(MAKE) all -C util_pkt_logger
	$(MAKE) all -C util_spi_stress
	$(MAKE) all -C util_tx_test

clean:
	$(MAKE) clean -C libloragw
	$(MAKE) clean -C util_band_survey
	$(MAKE) clean -C util_pkt_logger
	$(MAKE) clean -C util_spi_stress
	$(MAKE) clean -C util_tx_test

### EOF