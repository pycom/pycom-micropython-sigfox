#include "mbed.h"

DigitalOut myled(PB_1);
//Serial pc(USBTX,USBRX);
Serial pc(PB_6,PB_7);

void main (void)
{
pc.baud(9600);
  wait_ms(100);
   pc.printf ("clk source %d   \n",HAL_RCC_GetSysClockFreq());
  //     while(RCC_GetSYSCLKSource() != 0x04); // HSI 00, HSE 04, PLL 08

 myled = 1;
 wait(1);
    while(1) {
        myled = 1;
        pc.printf("coucou\n");
        wait(0.8);
        myled = 0;
        //wait(2.2);
    }
}

