### general build targets

all:
	$(MAKE) all -C libloragw
	$(MAKE) all -C loragw_band_survey
	$(MAKE) all -C loragw_pkt_logger
	$(MAKE) all -C loragw_spi_stress
	$(MAKE) all -C loragw_tx_test

clean:
	$(MAKE) clean -C libloragw
	$(MAKE) clean -C loragw_band_survey
	$(MAKE) clean -C loragw_pkt_logger
	$(MAKE) clean -C loragw_spi_stress
	$(MAKE) clean -C loragw_tx_test

### EOF