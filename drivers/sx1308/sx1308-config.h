
#ifndef _SX1308_CONFIG_H
#define _SX1308_CONFIG_H

#define SX1308_SPI_NUM                                          (SpiNum_SPI2)
#define SX1308_SCLK_PIN                                         (&PIN_MODULE_P10)
#define SX1308_MOSI_PIN                                         (&PIN_MODULE_P9)
#define SX1308_MISO_PIN                                         (&PIN_MODULE_P16)
#define SX1308_NSS_PIN                                          (&PIN_MODULE_P3)

#define SX1308_RST_PIN                                          (&PIN_MODULE_P11)
#define SX1308_TX_ON_PIN                                        (&PIN_MODULE_P14)
#define SX1308_RX_ON_PIN                                        (&PIN_MODULE_P15)

#define PYGATE_RADIO_A_EN_PIN                                   (&PIN_MODULE_P4)
#define PYGATE_RADIO_B_EN_PIN                                   (&PIN_MODULE_P21)
#define PYGATE_RF_POWER_EN_PIN                                  (&PIN_MODULE_P20)

#endif  // _SX1308_CONFIG_H
