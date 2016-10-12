/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2016 Semtech


Maintainer: Fabien Holin
*/
#ifndef SX1301_H
#define SX1301_H
#include "mbed.h"
class SX1301 
{
public:
    
   SX1301(PinName slaveSelectPin , PinName mosi, PinName miso, PinName sclk, PinName interrupt,PinName Reset);
   
    virtual bool   init();
    uint8_t        spiRead(uint8_t reg);
    void 		   dig_reset ();
    void           spiWrite(uint8_t reg, uint8_t val);
    void           spiWriteBurstF(uint8_t reg, int * val, int size);
void           spiWriteBurst(uint8_t reg, int * val, int size);
    void           spiWriteBurstM(uint8_t reg, int * val, int size);
    void           spiWriteBurstE(uint8_t reg, int * val, int size);
    uint8_t        spiReadBurstF(uint8_t reg,uint8_t *data,int size);
    uint8_t        spiReadBurstM(uint8_t reg,uint8_t *data,int size);
    uint8_t        spiReadBurstE(uint8_t reg,uint8_t *data,int size);
 uint8_t        spiReadBurst(uint8_t reg,uint8_t *data,int size);
    void           SelectPage(uint8_t reg);
    void           spiBurstRead(uint8_t reg, uint8_t* dest, uint8_t len);
    void           spiWriteMask(uint8_t reg,uint8_t val,uint8_t posmin,uint8_t mask) ;     
    void           setGPIO(int valuegp1,int valuegp2,int valuegp3,int valuegp4);
    void           irqmask(uint32_t mask);
    void           spiBurstWrite(uint8_t reg, const uint8_t* src, uint8_t len);
    bool           printRegisters();
    void 		   initmodem();
    void 		   initfifo(); 
  
private:
    void                 isr0();
    uint8_t             _interruptPin;
    DigitalOut          _slaveSelectPin;
    DigitalOut          _reset;
    SPI                 _spi;
    InterruptIn         _interrupt; 
};


#endif



