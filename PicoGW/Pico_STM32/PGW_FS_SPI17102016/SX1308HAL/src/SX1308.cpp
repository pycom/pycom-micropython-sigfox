/*
/ _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
\____ \| ___ |    (_   _) ___ |/ ___)  _ \
_____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
(C)2016 Semtech


Maintainer: Fabien Holin
*/
#include "SX1308.h"
#include "Registers1308.h"
#include "CmdUSB.h"
#include "mbed.h"
#include "board.h" 
#define DELAYSPI 1
SX1308::SX1308(PinName slaveSelectPin, PinName mosi, PinName miso, PinName sclk, PinName GPIO0, PinName Reset)
	: _slaveSelectPin(slaveSelectPin), _spi(mosi, miso, sclk), _interrupt(GPIO0), _reset(Reset)
{



}



bool SX1308::init()
{
#ifdef V2
	FEM_EN = 1;
#endif
	HSCLKEN = 1;
	_reset = 1;
	wait_ms(10);
	_reset = 0;
	wait_ms(10);

	_slaveSelectPin = 1;
	wait_ms(10);
	_spi.format(8, 0);
	_spi.frequency(8000000);
	// dig_reset();	//reset hw
	_interrupt.fall(this, &SX1308::isr0);
	txongoing = 0;
	hosttm = 0;
	offtmstpstm32 = 0;
	lgw_currentpage = 0;
	timerstm32ref.reset();
	timerstm32ref.start();
	firsttx = 0;

	return true;
}
void SX1308::dig_reset() //init modem for s2lp
{
	_reset = 1;
	 wait_us(10);
	_reset = 0;
	 wait_us(10);
}


void SX1308::isr0()
{
	__disable_irq();
	if (txongoing == 1)
	{
		waittxend = 0;
	}
	__enable_irq();
}

void SX1308::spiWrite(uint8_t reg, uint8_t val)
{
	__disable_irq();    // Disable Interrupts
	_slaveSelectPin = 0;
	wait_us(DELAYSPI);
	_spi.write(0x80 | (reg & 0x7F));
	_spi.write(val);
	wait_us(DELAYSPI);
	_slaveSelectPin = 1;
	__enable_irq();     // Enable Interrupts
}
void SX1308::spiWriteBurstF(uint8_t reg, int * val, int size)
{
	int i = 0;
	__disable_irq();    // Disable Interrupts
	_slaveSelectPin = 0;
	wait_us(DELAYSPI);
	_spi.write(0x80 | (reg & 0x7F));
	for (i = 0; i < size; i++)
	{
		_spi.write(val[i]);
	}


	__enable_irq();     // Enable Interrupts
}
void SX1308::spiWriteBurstM(uint8_t reg, int * val, int size)
{
	int i = 0;
	__disable_irq();    // Disable Interrupts



	for (i = 0; i < size; i++)
	{
		_spi.write(val[i]);
	}


	__enable_irq();     // Enable Interrupts
}
void SX1308::spiWriteBurstE(uint8_t reg, int * val, int size)
{
	int i = 0;
	__disable_irq();    // Disable Interrupts


	for (i = 0; i < size; i++)
	{
		_spi.write(val[i]);
	}
	wait_us(DELAYSPI);
	_slaveSelectPin = 1;
	__enable_irq();     // Enable Interrupts
}

void SX1308::spiWriteBurst(uint8_t reg, int * val, int size)
{
	int i = 0;
	__disable_irq();    // Disable Interrupts
	_slaveSelectPin = 0;
	wait_us(DELAYSPI);
	_spi.write(0x80 | (reg & 0x7F));

	for (i = 0; i < size; i++)
	{
		_spi.write(val[i]);
	}
	wait_us(DELAYSPI);
	_slaveSelectPin = 1;
	__enable_irq();     // Enable Interrupts
}

void SX1308::spiWriteBurstuint8(uint8_t reg, uint8_t * val, int size)
{
	int i = 0;
	__disable_irq();    // Disable Interrupts
	_slaveSelectPin = 0;
	wait_us(DELAYSPI);
	_spi.write(0x80 | (reg & 0x7F));

	for (i = 0; i < size; i++)
	{
		_spi.write(val[i]);
	}
	wait_us(DELAYSPI);
	_slaveSelectPin = 1;
	__enable_irq();     // Enable Interrupts
}

uint8_t SX1308::spiRead(uint8_t reg)
{
	__disable_irq();    // Disable Interrupts
	_slaveSelectPin = 0;
	wait_us(DELAYSPI);
	uint8_t val = 0;
	_spi.write(reg & 0x7F); // The written value is ignored, reg value is read
	val = _spi.write(0);
	wait_us(DELAYSPI);
	_slaveSelectPin = 1;
	__enable_irq();     // Enable Interrupts
	return val;
}
uint8_t SX1308::spiReadBurstF(uint8_t reg, uint8_t *data, int size) //first
{
	int i;
	__disable_irq();    // Disable Interrupts
	_slaveSelectPin = 0;
	wait_us(DELAYSPI);
	uint8_t val = 0;
	_spi.write(reg & 0x7F); // The written value is ignored, reg value is read
	for (i = 0; i < size; i++)
	{
		data[i] = _spi.write(0);
	}


	__enable_irq();     // Enable Interrupts
	return val;
}
uint8_t SX1308::spiReadBurstM(uint8_t reg, uint8_t *data, int size) //first
{  int i;
	__disable_irq();    // Disable Interrupts
	uint8_t val = 0;
	// The written value is ignored, reg value is read
	for (i = 0; i < size; i++)
	{
		data[i] = _spi.write(0);
	}
	__enable_irq();     // Enable Interrupts
	return val;
}
uint8_t SX1308::spiReadBurstE(uint8_t reg, uint8_t *data, int size) //first
{
	int i;
	__disable_irq();    // Disable Interrupts
	uint8_t val = 0;
	for (i = 0; i < size; i++)
	{
		data[i] = _spi.write(0);
	}
	wait_us(DELAYSPI);
	_slaveSelectPin = 1;
	__enable_irq();     // Enable Interrupts
	return val;
}
uint8_t SX1308::spiReadBurst(uint8_t reg, uint8_t *data, int size) //first
{
	int i;
	__disable_irq();    // Disable Interrupts
	_slaveSelectPin = 0;
	wait_us(10);
	uint8_t val = 0;
	_spi.write(reg & 0x7F);
	for (i = 0; i < size; i++)
	{
		data[i] = _spi.write(0);
	}
	wait_us(DELAYSPI);
	_slaveSelectPin = 1;
	__enable_irq();     // Enable Interrupts
	return val;
}


void SX1308::SelectPage(uint8_t reg)
{
	spiWrite(LGW_PAGE_REG, reg);
}
