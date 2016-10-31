/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2013 Semtech-Cycleo

Description:
    Host specific functions to address the LoRa concentrator registers through
    a SPI interface.
    Single-byte read/write and burst read/write.
    Does not handle pagination.
    Could be used with multiple SPI ports in parallel (explicit file descriptor)

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Sylvain Miermont
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>        /* C99 types */
#include <stdio.h>        /* printf fprintf */
#include <stdlib.h>        /* malloc free */
#include <unistd.h>        /* lseek, close */
#include <fcntl.h>        /* open */
#include <string.h>        /* memset */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <pthread.h>
#include "loragw_spi.h"
#include "loragw_hal.h"
#include "loragw_aux.h"

#include "loragw_reg.h"

#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <time.h>


#include <sys/select.h>
/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if DEBUG_SPI == 1
    #define DEBUG_MSG(str)                fprintf(stderr, str)
    #define DEBUG_PRINTF(fmt, args...)    fprintf(stderr,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
    #define CHECK_NULL(a)                if(a==NULL){fprintf(stderr,"%s:%d: ERROR: NULL POINTER AS ARGUMENT\n", __FUNCTION__, __LINE__);return LGW_SPI_ERROR;}
#else
    #define DEBUG_MSG(str)
    #define DEBUG_PRINTF(fmt, args...)
    #define CHECK_NULL(a)                if(a==NULL){return LGW_SPI_ERROR;}
#endif

/* -------------------------------------------------------------------------- */
/* --- PRIVATE SHARED VARIABLES (GLOBAL) ------------------------------------ */

pthread_mutex_t mx_usbbridgesync = PTHREAD_MUTEX_INITIALIZER; /* control access to usbbridge sync offsets */


/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define READ_ACCESS     0x00
#define WRITE_ACCESS    0x80
#define SPI_SPEED       8000000
#define SPI_DEV_PATH    "/dev/spidev0.0"
//#define SPI_DEV_PATH    "/dev/spidev32766.0"

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */


/* configure TTYACM0 port*/


int
set_interface_attribs (int fd, int speed, int parity)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                DEBUG_PRINTF ("error %d from tcgetattr", errno);
                return -1;
        }

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 50;            // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY|ICRNL); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
       //  tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
                DEBUG_PRINTF ("error %d from tcsetattr", errno);
                return -1;
        }
        return 0;
}
/* configure TTYACM0 read blocking or not*/
void set_blocking (int fd, int should_block)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                DEBUG_PRINTF ("error %d from tggetattr", errno);
                return ;
        }

        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 1;            // 0.5 seconds read timeout

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
               { DEBUG_PRINTF ("error %d setting term attributes", errno);}
}




/* SPI initialization and configuration */
int lgw_spi_open(void **spi_target_ptr) {
	
int *usb_device=NULL;
  /*check input variables*/
	CHECK_NULL(spi_target_ptr);	
	
    usb_device=malloc(sizeof(int));
    if (usb_device ==NULL){
		DEBUG_MSG("ERROR : MALLOC FAIL\n");
		return LGW_SPI_ERROR;
	}	
	
 /*TBD abstract the port name*/
 /*TBD fix the acm port */
    char *portname= "/dev/ttyACM0";
    int fd = open (portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0)
    {
        DEBUG_PRINTF ("ERROR: failed to open bridge USB /spi %s \n",portname);
      char *portname1 = "/dev/ttyACM1";
	 fd = open (portname1, O_RDWR | O_NOCTTY | O_SYNC);
       if (fd < 0)
        {
        DEBUG_PRINTF ("ERROR: failed to open bridge USB /spi %s \n",portname1);
     char *portname2 = "/dev/ttyACM2";
	 fd = open (portname2, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0)
    {
        DEBUG_PRINTF ("ERROR: failed to open bridge USB /spi %s \n",portname2);
    
    
     char   *portname3 = "/dev/ttyACM3";
	fd = open (portname3, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0)
    {    char *portname4 = "/dev/ttyACM4";
	      fd = open (portname4, O_RDWR | O_NOCTTY | O_SYNC);
	       if (fd < 0)
			{   char  *portname5 = "/dev/ttyACM5";
	      fd = open (portname5, O_RDWR | O_NOCTTY | O_SYNC);
	      if (fd < 0)
			{ 
        DEBUG_PRINTF ("ERROR: failed to open bridge USB /spi %s \n",portname5);
        return LGW_SPI_ERROR;
    }}}}}}

    set_interface_attribs (fd, B921600, 0);  // set speed to 115,200 bps, 8n1 (no parity)
    set_blocking (fd, 0);                // set  blocking
    *usb_device=fd;
    *spi_target_ptr=(void*)usb_device;
    return LGW_SPI_SUCCESS;

   

}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* SPI release */
int lgw_spi_close(void *spi_target) {
  int usb_device;
  int a;
   /*check input variables*/
	CHECK_NULL(spi_target);
	usb_device=*(int*)spi_target;
	
	a=close(usb_device);
	if (a<0){
		DEBUG_MSG("ERROR : USB PORT FAILED TO CLOSE\n");
		return LGW_SPI_ERROR;
	}
	else
	 {
		 DEBUG_MSG("Note : USB port closed \n");
		 return LGW_SPI_SUCCESS;
	 }
 }

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Simple write without ack TBD may be added ack */
int lgw_spi_w(void *spi_target, uint8_t spi_mux_mode, uint8_t spi_mux_target, uint8_t address, uint8_t data) {
    int fd;
 
    int temp4WARNING;
    temp4WARNING=spi_mux_mode;
    temp4WARNING=spi_mux_target;
    temp4WARNING++;
    fd = *(int *)spi_target; /* must check that spi_target is not null beforehand */
  
   /*build the write cmd*/
   CmdSettings_t mystruct;
   AnsSettings_t mystrctAns;
 
   mystruct.Cmd='w';
   mystruct.Id=0;
   mystruct.Len=1;
   mystruct.Adress=address;
   mystruct.Value[0]=data;
   pthread_mutex_lock(&mx_usbbridgesync);  
   SendCmdn(mystruct,fd) ;
   ReceiveAns(&mystrctAns,fd);
   pthread_mutex_unlock(&mx_usbbridgesync);
   DEBUG_MSG("Note: USB/SPI write success\n");
   return LGW_SPI_SUCCESS;
    
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Simple read */
int lgw_spi_r(void *spi_target, uint8_t spi_mux_mode, uint8_t spi_mux_target, uint8_t address, uint8_t *data) {
    int fd;
    
    int temp4WARNING;
    temp4WARNING=spi_mux_mode;
    temp4WARNING=spi_mux_target;
    temp4WARNING++;
    /* check input variables */
    CHECK_NULL(spi_target);
    CHECK_NULL(data);

    fd = *(int *)spi_target; /* must check that spi_target is not null beforehand */
    
   CmdSettings_t mystruct;
   AnsSettings_t mystrctAns;
   mystruct.Cmd='r';
   mystruct.Id=0;
   mystruct.Len=1;
   mystruct.Adress=address;
   mystruct.Value[0]=0;
   
   pthread_mutex_lock(&mx_usbbridgesync);  
   DEBUG_MSG("Note: SPI send cmd read success\n");
   SendCmdn(mystruct,fd) ;
   if(ReceiveAns(&mystrctAns,fd))
   {
	 DEBUG_MSG("Note: SPI read success\n");
     *data = mystrctAns.Rxbuf[0];
      pthread_mutex_unlock(&mx_usbbridgesync);
      return LGW_SPI_SUCCESS;
   }
   else
   {
        DEBUG_MSG("ERROR: SPI READ FAILURE\n");
         pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_SPI_ERROR;
    } 
    
}


int lgw_spi_wb(void *spi_target, uint8_t spi_mux_mode, uint8_t spi_mux_target, uint8_t address, uint8_t *data, uint16_t size) {
    int fd;
    int i;
	int sizei=size;
	int cptalc=0;
	int temp4WARNING;
	temp4WARNING=spi_mux_mode;
    temp4WARNING=spi_mux_target;
    temp4WARNING++;
  
	CmdSettings_t mystruct;
	AnsSettings_t mystrctAns;
	
    /* check input parameters */
    CHECK_NULL(spi_target);
    fd = *(int *)spi_target; /* must check that spi_target is not null beforehand */

    /* prepare command byte */
  pthread_mutex_lock(&mx_usbbridgesync);  
 while (sizei>ATOMICTX) // 64 bytes fifo transfer TBD have to be improve to speed up
 {
	  if (sizei==size)
   {	 
   mystruct.Cmd='x';
   }
   else
    {	 
   mystruct.Cmd='y';
    }
  
   mystruct.Id=(ATOMICTX>>8);
   mystruct.Len=ATOMICTX-((ATOMICTX>>8)<<8);
   mystruct.Adress=address;
  
   for (i=0;i<ATOMICTX;i++)
   {
   
   mystruct.Value[i]=data[i+cptalc];
   }
   
   SendCmdn(mystruct,fd) ;
   ReceiveAns(&mystrctAns,fd);
   sizei=sizei-ATOMICTX;
   
   cptalc=cptalc+ATOMICTX;
  
}
/*end of the transfer*/
if (sizei>0)
{
		 if (size<=ATOMICTX)
	 {  
   mystruct.Cmd='a';
}
else
{
	mystruct.Cmd='z';
}
	
   mystruct.Id=(sizei>>8);
   mystruct.Len=sizei-((sizei>>8)<<8);
   mystruct.Adress=address;
  
   for (i=0;i<((mystruct.Id<<8)+mystruct.Len);i++)
   {
   
   mystruct.Value[i]=data[i+cptalc];
   }
 
   SendCmdn(mystruct,fd) ;
   ReceiveAns(&mystrctAns,fd);
   pthread_mutex_unlock(&mx_usbbridgesync);  
  
} 	   
        DEBUG_MSG("Note: SPI burst write success\n");
        return LGW_SPI_SUCCESS;   
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


/* Burst (multiple-byte) read */
int lgw_spi_rb(void *spi_target, uint8_t spi_mux_mode, uint8_t spi_mux_target, uint8_t address, uint8_t *data, uint16_t size) {
    int fd;
   int temp4WARNING;
	temp4WARNING=spi_mux_mode;
    temp4WARNING=spi_mux_target;
    temp4WARNING++;

    /* check input parameters */
    CHECK_NULL(spi_target);
    
    fd = *(int *)spi_target; /* must check that spi_target is not null beforehand */

   
	int i;
   CmdSettings_t mystruct;
   AnsSettings_t mystrctAns;
 
   int sizei=size;
	int cptalc=0;
	 pthread_mutex_lock(&mx_usbbridgesync);  
 while (sizei>ATOMICRX)
 { 
   if (sizei==size)
   {	 
   mystruct.Cmd='s';
   }
   else
    {	 
   mystruct.Cmd='t';
    }
  
   mystruct.Id=0;
   mystruct.Len=2;
   mystruct.Value[0]=ATOMICRX>>8;
   mystruct.Value[1]= ATOMICRX-((ATOMICRX>>8)<<8);
   mystruct.Adress=address;
   
  
   SendCmdn(mystruct,fd) ;	   
   if(ReceiveAns(&mystrctAns,fd))
   {
	   for (i=0;i<ATOMICRX;i++)
	   {
		   data[i+cptalc]=mystrctAns.Rxbuf[i];
	   }
   }
   else
   {
	   
	   for (i=0;i<ATOMICRX;i++)
	   {
		   data[i+cptalc]=0xFF;
	   }
   }
  
    sizei=sizei-ATOMICRX;
   cptalc=cptalc+ATOMICRX;
 
   }
 if (sizei>0){
	 if (size<=ATOMICRX)
	 {  
   mystruct.Cmd='p';
}
else
{
	mystruct.Cmd='u';
}
   mystruct.Id=0;
   mystruct.Len=2;
   mystruct.Value[0]=sizei>>8;
   mystruct.Value[1]= sizei-((sizei>>8)<<8);
   mystruct.Adress=address;
  
   DEBUG_MSG("Note: SPI send cmd readburst success\n"); 
   SendCmdn(mystruct,fd) ;

   if(ReceiveAns(&mystrctAns,fd))
   {
	   for (i=0;i<sizei;i++)
	   {
		   data[i+cptalc]=mystrctAns.Rxbuf[i];
	   }
	   pthread_mutex_unlock(&mx_usbbridgesync);  
       return LGW_SPI_SUCCESS;
   }
   else
   {    DEBUG_MSG("ERROR: Cannot readburst stole  \n");
	   
	   
	    pthread_mutex_unlock(&mx_usbbridgesync);  
	    return LGW_SPI_ERROR;
	   
   }
   

}else
{return LGW_SPI_ERROR;
}
        
}

int SendCmdn(CmdSettings_t CmdSettings,int file1) 	
{    
	char buffertx[BUFFERTXSIZE];
	int Clen =  CmdSettings.Len+(CmdSettings.Id<<8);
	int Tlen = 1+2+1+Clen; // cmd  Length +adress
	int i;
	for (i=0;i<BUFFERTXSIZE;i++)
	{
		 buffertx[i]=0;
	}
	
    buffertx[0]=CmdSettings.Cmd;
    buffertx[1]=CmdSettings.Id;
    buffertx[2]=CmdSettings.Len;
    buffertx[3]=CmdSettings.Adress;
    for (i=0;i<Clen;i++)   
	{
		buffertx[i+4]=CmdSettings.Value[i];
		
	}
    write(file1,buffertx,Tlen);
    DEBUG_PRINTF("send burst done size %d\n",Tlen);
	return(1); //tbd	
}


int ReceiveAns(AnsSettings_t *Ansbuffer,int file1) 	
{
	uint8_t bufferrx[BUFFERRXSIZE];
    int i;
    int cpttimer=0;
    for (i=0;i<BUFFERRXSIZE;i++)
    {
		bufferrx[i]=0;
	}
// while((bufferrx[0]==0))
// {
// 	read(file1,bufferrx,1);
// 	cpttimer++;

// }
 cpttimer=0; 
 
	//while((((((bufferrx[1]<<8)+bufferrx[2])==0)||(((bufferrx[1]<<8)+bufferrx[2])>ATOMICRX+6))) || (
  while(checkcmd(bufferrx[0]))

	{
	read(file1,bufferrx,3); 
	cpttimer++;
	//wait_ms(1);
  
	if (cpttimer>15) // wait read error the read function isn't block but timeout of 0.1s
	{DEBUG_MSG("ERROR: DEADLOCK SPI");
  return(0); // deadlock
	}
    }
  
	wait_ns(((bufferrx[1]<<8)+bufferrx[2])*4000);
	DEBUG_PRINTF("cmd = %d readburst size %d\n",bufferrx[0],(bufferrx[1]<<8)+bufferrx[2]);
	read(file1,&bufferrx[3],(bufferrx[1]<<8)+bufferrx[2]);
	
	//fread(&bufferrx[3],sizeof(uint8_t),bufferrx[2],file1);
	for (i=0;i<10;i++)
	{	//printf ("%.2x ",bufferrx[i]);
	}
	//printf ("\n");
	Ansbuffer->Cmd=bufferrx[0];
	Ansbuffer->Id=bufferrx[1];
	Ansbuffer->Len=bufferrx[2];
	for(i=0;i<(bufferrx[1]<<8)+bufferrx[2];i++)
	{
	Ansbuffer->Rxbuf[i]=bufferrx[3+i];
 
    }
   
     if (bufferrx[0]==0)
     {
     for(i=0;i<100;i++){
     DEBUG_PRINTF("buffer[%d]=%d\n",i,bufferrx[i]);
     }
     }
	return(1);
}

int ReceiveAnsCmd(AnsSettings_t *Ansbuffer,int file1,uint8_t cmd) 	
{
	uint8_t bufferrx[BUFFERRXSIZE];
    int i;
    int cpttimer=0;
    for (i=0;i<BUFFERRXSIZE;i++)
    {
		bufferrx[i]=0;
	}
 cpttimer=0; 
  while(bufferrx[0]!=cmd)
	{
	read(file1,bufferrx,3); 
	cpttimer++;
	if (cpttimer>5) // wait read error the read function isn't block but timeout of 0.1s
	{
 	read(file1,&bufferrx[3],ATOMICRX);// try to purge
  DEBUG_MSG("ERROR:  WRONG SPI CMD");
  return(LGW_SPI_ERROR); // deadlock
	}
 }
  
	wait_ns(((bufferrx[1]<<8)+bufferrx[2])*4000);
	DEBUG_PRINTF("cmd = %d readburst size %d\n",bufferrx[0],(bufferrx[1]<<8)+bufferrx[2]);
 
  if (((bufferrx[1]<<8)+bufferrx[2])<ATOMICRX)
  {
	read(file1,&bufferrx[3],(bufferrx[1]<<8)+bufferrx[2]);
	}
  else 
  {	read(file1,&bufferrx[3],ATOMICRX);// try to purge
    DEBUG_MSG("ERROR: WRONG SPI SIZE");
    return(LGW_SPI_ERROR) ;
  }
	//fread(&bufferrx[3],sizeof(uint8_t),bufferrx[2],file1);
	
	Ansbuffer->Cmd=bufferrx[0];
	Ansbuffer->Id=bufferrx[1];
	Ansbuffer->Len=bufferrx[2];
	for(i=0;i<(bufferrx[1]<<8)+bufferrx[2];i++)
	{
	Ansbuffer->Rxbuf[i]=bufferrx[3+i];
 
    }
  if (bufferrx[2+i]!=0x09){DEBUG_PRINTF("ERROR: WRONG SPI PARITY CHECK %x  \n",bufferrx[3+i]);return(LGW_SPI_ERROR);}
  if (bufferrx[1+i]!=0x20){DEBUG_PRINTF("ERROR: WRONG SPI PARITY CHECK %x \n",bufferrx[2+i] );return(LGW_SPI_ERROR);}
  if (bufferrx[i]!=0x04){DEBUG_PRINTF("ERROR: WRONG SPI PARITY CHECK %x \n",bufferrx[1+i] );return(LGW_SPI_ERROR);}
  if (bufferrx[i-1]!=0x23){DEBUG_PRINTF("ERROR: WRONG SPI PARITY CHECK %x \n",bufferrx[i] );return(LGW_SPI_ERROR);}
   
	return(LGW_SPI_SUCCESS);
}



/*Embedded HAL into STM32 part */

int lgw_receive_cmd(void *spi_target, uint8_t max_packet, uint8_t *data) {
    int fd;
    int i;
    int j;
    int pt=0;
    int resp=0;
    fd = *(int *)spi_target; /* must check that spi_target is not null beforehand */
  
   /*build the write cmd*/
   CmdSettings_t mystruct;
   AnsSettings_t mystrctAns;
 
   mystruct.Cmd='b';
   mystruct.Id=0;
   mystruct.Len=1;
   mystruct.Adress=0;
   mystruct.Value[0]=max_packet;
   pthread_mutex_lock(&mx_usbbridgesync);  
   SendCmdn(mystruct,fd) ;
   resp=ReceiveAnsCmd(&mystrctAns,fd,'b');
  
   DEBUG_MSG("Note: USB/SPI write success\n");
   DEBUG_PRINTF("NOTE : Available packet %d  %d\n",mystrctAns.Rxbuf[0],(mystrctAns.Id<<8)+ mystrctAns.Len);
   DEBUG_PRINTF("NOTE : read structure %d %d %d %d %d\n",mystrctAns.Rxbuf[5],mystrctAns.Rxbuf[6],mystrctAns.Rxbuf[7],mystrctAns.Rxbuf[8],mystrctAns.Rxbuf[9]);
   
   int cptalc=0;
   if (resp ==LGW_SPI_ERROR)
   { pthread_mutex_unlock(&mx_usbbridgesync); 
     return (0); // for 0 receive packet 
   }
   
   for (i=0;i<mystrctAns.Rxbuf[0];i++) // over the number of packets
   {
   for (j=0;j<mystrctAns.Rxbuf[cptalc+43]+44;j++) // for each packet
   {
   pt=mystrctAns.Rxbuf[cptalc+43]+44;
   data[(i*300)+j]=mystrctAns.Rxbuf[j+cptalc+1];//300 size of struct target
   }
   cptalc=pt;
   
   }
    
  pthread_mutex_unlock(&mx_usbbridgesync);
  return mystrctAns.Rxbuf[0];
    
}



/*Embedded HAL into STM32 part */

int lgw_rxrf_setconfcmd(void *spi_target, uint8_t rfchain, uint8_t *data,uint16_t size) {
    int fd;
    int i;
    DEBUG_MSG("Note: USB/SPI write success\n");
    fd = *(int *)spi_target; /* must check that spi_target is not null beforehand */
   DEBUG_PRINTF("Note: USB/SPI write success %d\n",fd);
   /*build the write cmd*/
   CmdSettings_t mystruct;
   AnsSettings_t mystrctAns;
 
   mystruct.Cmd='c';
   mystruct.Id=(size>>8);
   mystruct.Len=size-((size>>8)<<8);
   mystruct.Adress=rfchain;
    DEBUG_PRINTF("Note: USB/SPI write success size = %d\n",size);
    DEBUG_MSG("Note: USB/SPI write success\n");
   for (i=0;i<size;i++)
   {
   mystruct.Value[i]=data[i];
   }
    DEBUG_MSG("Note: USB/SPI write success\n");
   pthread_mutex_lock(&mx_usbbridgesync);  
   SendCmdn(mystruct,fd) ;
   if(ReceiveAns(&mystrctAns,fd))
   { DEBUG_MSG("Note: USB/SPI read config success\n");
   pthread_mutex_unlock(&mx_usbbridgesync);
   return LGW_SPI_SUCCESS;
	}
	else
	{DEBUG_MSG("ERROR: USB/SPI read config FAILED\n");
  pthread_mutex_unlock(&mx_usbbridgesync);
   return LGW_SPI_ERROR;
	}
}


int lgw_rxif_setconfcmd(void *spi_target, uint8_t ifchain, uint8_t *data,uint16_t size) {
    int fd;
    int i;
    DEBUG_MSG("Note: USB/SPI write success\n");
    fd = *(int *)spi_target; /* must check that spi_target is not null beforehand */
   DEBUG_PRINTF("Note: USB/SPI write success %d\n",fd);
   /*build the write cmd*/
   CmdSettings_t mystruct;
   AnsSettings_t mystrctAns;
 
   mystruct.Cmd='d';
   mystruct.Id=(size>>8);
   mystruct.Len=size-((size>>8)<<8);
   mystruct.Adress=ifchain;
    DEBUG_PRINTF("Note: USB/SPI write success size = %d\n",size);
    DEBUG_MSG("Note: USB/SPI write success\n");
   for (i=0;i<size;i++)
   {
   mystruct.Value[i]=data[i];
   }
    DEBUG_MSG("Note: USB/SPI write success\n");
   pthread_mutex_lock(&mx_usbbridgesync);  
   SendCmdn(mystruct,fd) ;
   if(ReceiveAns(&mystrctAns,fd))
   { DEBUG_MSG("Note: USB/SPI read config success\n");
   pthread_mutex_unlock(&mx_usbbridgesync);
   return LGW_SPI_SUCCESS;
	}
	else
	{DEBUG_MSG("ERROR: USB/SPI read config FAILED\n");
  pthread_mutex_unlock(&mx_usbbridgesync);
   return LGW_SPI_ERROR;
	}
}

int lgw_txgain_setconfcmd(void *spi_target, uint8_t *data,uint16_t size)
 { int fd;
    int i;
    DEBUG_MSG("Note: USB/SPI write success\n");
    fd = *(int *)spi_target; /* must check that spi_target is not null beforehand */
   DEBUG_PRINTF("Note: USB/SPI write success %d\n",fd);
   /*build the write cmd*/
   CmdSettings_t mystruct;
   AnsSettings_t mystrctAns;
 
   mystruct.Cmd='h';
   mystruct.Id=(size>>8);
   mystruct.Len=size-((size>>8)<<8);
   mystruct.Adress=0;
    DEBUG_PRINTF("Note: USB/SPI write success size = %d\n",size);
    DEBUG_MSG("Note: USB/SPI write success\n");
   for (i=0;i<size;i++)
   {
   mystruct.Value[i]=data[i];
 
   }
    DEBUG_MSG("Note: USB/SPI write success\n");
   pthread_mutex_lock(&mx_usbbridgesync);  
   SendCmdn(mystruct,fd) ;
   if(ReceiveAns(&mystrctAns,fd))
   { DEBUG_MSG("Note: USB/SPI read config success\n");
   pthread_mutex_unlock(&mx_usbbridgesync);
   return LGW_SPI_SUCCESS;
	}
	else
	{DEBUG_MSG("ERROR: USB/SPI read config FAILED\n");
  pthread_mutex_unlock(&mx_usbbridgesync);
   return LGW_SPI_ERROR;
	}
}



int lgw_sendconfcmd(void *spi_target,uint8_t *data,uint16_t size) {
    int fd;
    int i;
    DEBUG_MSG("Note SEND A PACKET: USB/SPI write success\n");
    fd = *(int *)spi_target; /* must check that spi_target is not null beforehand */
   DEBUG_PRINTF("Note: USB/SPI write success %d\n",fd);
   /*build the write cmd*/
   CmdSettings_t mystruct;
   AnsSettings_t mystrctAns;
 
   mystruct.Cmd='f';
   mystruct.Id=(size>>8);
   mystruct.Len=size-((size>>8)<<8);
   mystruct.Adress=0;
    DEBUG_PRINTF("Note: USB/SPI write success size = %d\n",size);
    DEBUG_MSG("Note: USB/SPI write success\n");
   for (i=0;i<size;i++)
   {
   mystruct.Value[i]=data[i];
   // DEBUG_PRINTF("debug data[%d]=%d\n",i, mystruct.Value[i]);
   }
    DEBUG_MSG("Note: USB/SPI write success\n");
   pthread_mutex_lock(&mx_usbbridgesync);  
   SendCmdn(mystruct,fd) ;
   if(ReceiveAns(&mystrctAns,fd))
   { DEBUG_MSG("Note: USB/SPI read config success\n");
   pthread_mutex_unlock(&mx_usbbridgesync);
   return LGW_SPI_SUCCESS;
	}
	else
	{DEBUG_MSG("ERROR: USB/SPI read config FAILED\n");
  pthread_mutex_unlock(&mx_usbbridgesync);
   return LGW_SPI_ERROR;
	}
}



int lgw_trigger(void *spi_target, uint8_t address, uint32_t *data) {
    int fd;
    
   
    /* check input variables */
    CHECK_NULL(spi_target);
    CHECK_NULL(data);

    fd = *(int *)spi_target; /* must check that spi_target is not null beforehand */
    
   CmdSettings_t mystruct;
   AnsSettings_t mystrctAns;
   mystruct.Cmd='q';
   mystruct.Id=0;
   mystruct.Len=1;
   mystruct.Adress=address;
   mystruct.Value[0]=0;
   
   pthread_mutex_lock(&mx_usbbridgesync);  
   DEBUG_MSG("Note: SPI send cmd read success\n");
   SendCmdn(mystruct,fd) ;
   if(ReceiveAns(&mystrctAns,fd))
   {
	 DEBUG_MSG("Note: SPI read success\n");
     *data =(mystrctAns.Rxbuf[0]<<24)+(mystrctAns.Rxbuf[1]<<16)+(mystrctAns.Rxbuf[2]<<8)+(mystrctAns.Rxbuf[3]);
     DEBUG_PRINTF("timestampreceive %d\n",(mystrctAns.Rxbuf[0]<<24)+(mystrctAns.Rxbuf[1]<<16)+(mystrctAns.Rxbuf[2]<<8)+(mystrctAns.Rxbuf[3]));
      pthread_mutex_unlock(&mx_usbbridgesync);
      return LGW_SPI_SUCCESS;
   }
   else
   {
        DEBUG_MSG("ERROR: SPI READ FAILURE\n");
         pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_SPI_ERROR;
    } 
    
}




/****************************/
int checkcmd(uint8_t cmd)
{
 switch (cmd)
 {
 case 'r':{return(0); break;}
 case 's':{return(0); break;}
 case 't':{return(0); break;}
 case 'u':{return(0); break;}
 case 'p':{return(0); break;}
 case 'e':{return(0); break;}
 case 'w':{return(0); break;}
 case 'x':{return(0); break;}
 case 'y':{return(0); break;}
 case 'z':{return(0); break;}
 case 'a':{return(0); break;}
 case 'b':{return(0); break;}
 case 'c':{return(0); break;}
 case 'd':{return(0); break;}
 case 'f':{return(0); break;}
 case 'h':{return(0); break;}
 case 'q':{return(0); break;}
 //case 97 : return (1);   
     
     default : 
     return(1);
  } 
  return(1);  
}


/* --- EOF ------------------------------------------------------------------ */
