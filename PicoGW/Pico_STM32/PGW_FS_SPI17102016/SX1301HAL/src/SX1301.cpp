/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2016 Semtech


Maintainer: Fabien Holin
*/
#include "SX1301.h"
#include "Registers1301.h"
#include "CmdUSB.h"
#include "mbed.h"
#include "board.h" 
#define DELAYSPI 1
SX1301::SX1301(PinName slaveSelectPin, PinName mosi, PinName miso, PinName sclk, PinName GPIO0,PinName Reset)
       : _slaveSelectPin(slaveSelectPin),  _spi(mosi, miso, sclk), _interrupt(GPIO0),_reset(Reset)
{		 

	
	 
}



bool SX1301::init()
	{ 
		#ifdef V2
		FEM_EN =1;
		HSCLKEN=1;
		#endif
		_reset=1;
		wait_ms(10);
		_reset=0;
		wait_ms(1);
		 _reset=1;
		wait_ms(10);
		_reset=0;
		wait_ms(1);
		 _reset=1;
		wait_ms(10);
		_reset=0;
		wait_ms(10);
		#ifdef V2
		//RADIO_RST=1;
		wait_ms(10);
		//RADIO_RST=0;
		#endif
		_slaveSelectPin = 1;
			wait_ms(10);
		 _spi.format(8,0);
		 _spi.frequency(8000000);
		 // dig_reset();	//reset hw
			_interrupt.fall(this, &SX1301::isr0);
		txongoing=0;
		hosttm=0;
		offtmstpstm32=0;
		lgw_currentpage=0;
		timerstm32ref.reset();
		timerstm32ref.start();
		firsttx=0;
		#ifdef V2
		FEM_EN =1;
		HSCLKEN=1;
		#endif
		return true; 
	}	
void SX1301::dig_reset() //init modem for s2lp
{ 
 _reset=1;
		wait_us(10);
_reset=0;  
//	wait_us(10);
}
	
void SX1301::initmodem() //init modem for s2lp
{ 
   
}
void SX1301::isr0()
{
//pc.printf("it done\n");
	
	 __disable_irq();  
	 if (txongoing==1)
	 {
		 waittxend=0;
	 } 
	
	 __enable_irq();  
}

void SX1301::spiWrite(uint8_t reg, uint8_t val) 
{
    __disable_irq();    // Disable Interrupts
    _slaveSelectPin = 0;
	wait_us(DELAYSPI);
    _spi.write(0x80|(reg&0x7F));
    _spi.write(val);
	wait_us(DELAYSPI);
    _slaveSelectPin = 1;
    __enable_irq();     // Enable Interrupts
}
void SX1301::spiWriteBurstF(uint8_t reg, int * val, int size) 
{
	int i =0;
    __disable_irq();    // Disable Interrupts
    _slaveSelectPin = 0;
	wait_us(DELAYSPI);
    _spi.write(0x80|(reg&0x7F));
	for (i=0;i<size;i++)
	{
    _spi.write(val[i]);
	}
	
  
    __enable_irq();     // Enable Interrupts
}
void SX1301::spiWriteBurstM(uint8_t reg, int * val, int size) 
{
	int i =0;
    __disable_irq();    // Disable Interrupts
   


	for (i=0;i<size;i++)
	{
    _spi.write(val[i]);
	}
	
   
    __enable_irq();     // Enable Interrupts
}
void SX1301::spiWriteBurstE(uint8_t reg, int * val, int size) 
{
	int i =0;
    __disable_irq();    // Disable Interrupts
  
 
	for (i=0;i<size;i++)
	{
    _spi.write(val[i]);
	}
	wait_us(DELAYSPI);
    _slaveSelectPin = 1;
    __enable_irq();     // Enable Interrupts
}

void SX1301::spiWriteBurst(uint8_t reg, int * val, int size) 
{
	int i =0;
    __disable_irq();    // Disable Interrupts
    _slaveSelectPin = 0;
	wait_us(DELAYSPI);
    _spi.write(0x80|(reg&0x7F));
 
	for (i=0;i<size;i++)
	{
    _spi.write(val[i]);
	}
	wait_us(DELAYSPI);
    _slaveSelectPin = 1;
    __enable_irq();     // Enable Interrupts
}

void SX1301::spiWriteBurstuint8(uint8_t reg, uint8_t * val, int size) 
{
	int i =0;
    __disable_irq();    // Disable Interrupts
    _slaveSelectPin = 0;
	wait_us(DELAYSPI);
    _spi.write(0x80|(reg&0x7F));
 
	for (i=0;i<size;i++)
	{
    _spi.write(val[i]);
	}
	wait_us(DELAYSPI);
    _slaveSelectPin = 1;
    __enable_irq();     // Enable Interrupts
}

uint8_t SX1301::spiRead(uint8_t reg)
{
    __disable_irq();    // Disable Interrupts
    _slaveSelectPin=0;
	wait_us(DELAYSPI);
    uint8_t val=0;
	  _spi.write(reg&0x7F); // The written value is ignored, reg value is read
	  val=_spi.write(0);
	wait_us(DELAYSPI);
    _slaveSelectPin = 1;
    __enable_irq();     // Enable Interrupts
    return val;
}
uint8_t SX1301::spiReadBurstF(uint8_t reg,uint8_t *data,int size) //first
{int i;
    __disable_irq();    // Disable Interrupts
    _slaveSelectPin=0;
	wait_us(DELAYSPI);
    uint8_t val=0;
	  _spi.write(reg&0x7F); // The written value is ignored, reg value is read
	for (i=0;i<size;i++)
	{
	  data[i]=_spi.write(0);
	}
	
    
    __enable_irq();     // Enable Interrupts
    return val;
}
uint8_t SX1301::spiReadBurstM(uint8_t reg,uint8_t *data,int size) //first
{int i;
    __disable_irq();    // Disable Interrupts
   
	
    uint8_t val=0;
	  // The written value is ignored, reg value is read
	for (i=0;i<size;i++)
	{
	  data[i]=_spi.write(0);
	}
	
  
    __enable_irq();     // Enable Interrupts
    return val;
}
uint8_t SX1301::spiReadBurstE(uint8_t reg,uint8_t *data,int size) //first
{int i;
    __disable_irq();    // Disable Interrupts
    

    uint8_t val=0;
	 
	for (i=0;i<size;i++)
	{
	  data[i]=_spi.write(0);
	}
	wait_us(DELAYSPI);
    _slaveSelectPin = 1;
    __enable_irq();     // Enable Interrupts
    return val;
}
uint8_t SX1301::spiReadBurst(uint8_t reg,uint8_t *data,int size) //first
{int i;
    __disable_irq();    // Disable Interrupts
      _slaveSelectPin=0;
	wait_us(10);
    uint8_t val=0;
	  _spi.write(reg&0x7F);

    
	 
	for (i=0;i<size;i++)
	{
	  data[i]=_spi.write(0);
	}
	wait_us(DELAYSPI);
    _slaveSelectPin = 1;
    __enable_irq();     // Enable Interrupts
    return val;
}


void SX1301::SelectPage(uint8_t reg)
{
	spiWrite(LGW_PAGE_REG, reg); 
	
}
		void SX1301::resetsx1257(void)
{
			
}
