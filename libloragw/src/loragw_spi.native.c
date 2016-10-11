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

#include "loragw_spi.h"
#include "loragw_hal.h"
#include "loragw_aux.h"


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
                printf ("error %d from tcgetattr", errno);
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
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY|ICRNL); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
       //  tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
                printf ("error %d from tcsetattr", errno);
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
                printf ("error %d from tggetattr", errno);
                return;
        }

        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
                printf ("error %d setting term attributes", errno);
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
    char *portname = "/dev/ttyACM0";
	int fd = open (portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0)
    {
        DEBUG_PRINTF ("ERROR: failed to open bridge USB /spi %s \n",portname);
        return LGW_SPI_ERROR;
    }

    set_interface_attribs (fd, B115200, 0);  // set speed to 115,200 bps, 8n1 (no parity)
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
   mystruct.Cmd='w';
   mystruct.Id=0;
   mystruct.Len=1;
   mystruct.Adress=address;
   mystruct.Value[0]=data;
   SendCmd(mystruct,fd) ;
   wait_ms(2);
       
    /* TBD + failure cases determine return code */
  
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
   SendCmd(mystruct,fd) ;
   wait_ms(2);	   
   if(ReceiveAns(&mystrctAns,fd))
   {
	 DEBUG_MSG("Note: SPI read success\n");
     *data = mystrctAns.Rxbuf[0];
     
      return LGW_SPI_SUCCESS;
   }
   else
   {
        DEBUG_MSG("ERROR: SPI READ FAILURE\n");
        return LGW_SPI_ERROR;
    } 
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Burst (multiple-byte) write */
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

    /* check input parameters */
    CHECK_NULL(spi_target);
    fd = *(int *)spi_target; /* must check that spi_target is not null beforehand */

    /* prepare command byte */
 
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
  
   mystruct.Id=0;
   mystruct.Len=ATOMICTX;
   mystruct.Adress=address;
  
   for (i=0;i<mystruct.Len;i++)
   {
   
   mystruct.Value[i]=data[i+cptalc];
   }
   SendCmd(mystruct,fd) ;
   sizei=sizei-ATOMICTX;
 
   cptalc=cptalc+ATOMICTX;
   wait_ms(1);
}
/*end of the transfer*/
if (sizei>0)
{
		 if (size<ATOMICTX)
	 {  
   mystruct.Cmd='a';
}
else
{
	mystruct.Cmd='z';
}
	
   mystruct.Id=0;
   mystruct.Len=sizei;
   mystruct.Adress=address;
  
   for (i=0;i<mystruct.Len;i++)
   {
   
   mystruct.Value[i]=data[i+cptalc];
   }
 
   SendCmd(mystruct,fd) ;
  wait_ms(2);
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
   mystruct.Adress=address;
   
   mystruct.Value[0]=0;
   mystruct.Value[1]= ATOMICRX;
//   printf("le= %d et %d\n", mystruct.Value[0], mystruct.Value[1]);
   SendCmd(mystruct,fd) ;
   wait_ms(1);	   
   if(ReceiveAns(&mystrctAns,fd))
   {
	   for (i=0;i<ATOMICRX;i++)
	   {data[i+cptalc]=mystrctAns.Rxbuf[i];
	   }
	 //  printf("struc receiv %d %d %d %d \n",mystrctAns.Cmd,mystrctAns.Id,mystrctAns.Len,mystrctAns.Rxbuf[0]);
   }
   sizei=sizei-ATOMICRX;
 
   cptalc=cptalc+ATOMICRX;
   wait_ms(2);	
   }
 if (sizei>0){
	 if (size<ATOMICRX)
	 {  
   mystruct.Cmd='p';
}
else
{
	mystruct.Cmd='u';
}
   mystruct.Id=0;
   mystruct.Len=2;
   mystruct.Adress=address;
   
   mystruct.Value[0]=0;
   mystruct.Value[1]= sizei;
//   printf("le= %d et %d\n", mystruct.Value[0], mystruct.Value[1]);
   SendCmd(mystruct,fd) ;
   wait_ms(1);	   
   if(ReceiveAns(&mystrctAns,fd))
   {
	   for (i=0;i<sizei;i++)
	   {data[i+cptalc]=mystrctAns.Rxbuf[i];
	   }
	 //  printf("struc receiv %d %d %d %d \n",mystrctAns.Cmd,mystrctAns.Id,mystrctAns.Len,mystrctAns.Rxbuf[0]);
   }
   wait_ms(1);	
}
  
        DEBUG_MSG("Note: SPI burst read success\n");
        return LGW_SPI_SUCCESS;
   
}


/*usb addded*/


int SendCmd(CmdSettings_t CmdSettings,int file1) 	
{     
	char buffertx[BUFFERTXSIZE];
	
	int Clen =  CmdSettings.Len;
	
	int Tlen = 1+2+2+2+Clen*2; // cmd +ID+ Length =2chars+ write/read = 4*Clen
	
	if (Clen>255)
	{
	 return(-1); 	
	}
	int i;
	int adresstemp;
	int valuetemp;
	
	for (i=0;i<BUFFERTXSIZE;i++)
	{
		 buffertx[i]='\n';
	}
	
    buffertx[0]=CmdSettings.Cmd;
    
    sprintf(&buffertx[1],"%.2x%.2x",CmdSettings.Id,CmdSettings.Len);
    adresstemp=CmdSettings.Adress;
    sprintf(&buffertx[5],"%.2x",adresstemp);
    for (i=0;i<(Clen);i++)   
	{
		
		valuetemp=CmdSettings.Value[i];
		
		sprintf(&buffertx[2*i+7],"%.2x",valuetemp);
	}
	#if debug
	{
	// printf("buffertx=%s\n",&buffertx[0]);
	}
    #endif
    write(file1,buffertx,Tlen+4);
	//fwrite(buffertx,sizeof(char),Tlen+4,file1);
	return(1); //tbd	
}

int ReceiveAns(AnsSettings_t *Ansbuffer,int file1) 	
{
	uint8_t bufferrx[BUFFERRXSIZE];
    int i;
    for (i=0;i<BUFFERRXSIZE;i++)
    {
		bufferrx[i]=0;
	
	}
	read(file1,bufferrx,3);
	//fread(bufferrx,sizeof(uint8_t),3,file1);
	for (i=0;i<3;i++)
	{//	printf ("\n %.2x ",bufferrx[i]);
	}
	
	read(file1,&bufferrx[3],(bufferrx[1]<<8)+bufferrx[2]);
	//fread(&bufferrx[3],sizeof(uint8_t),bufferrx[2],file1);
	for (i=0;i<10;i++)
	{	//printf ("%.2x ",bufferrx[i]);
	}
	//printf ("\n");
	Ansbuffer->Cmd=bufferrx[0];
	Ansbuffer->Id=bufferrx[1];
	Ansbuffer->Len=bufferrx[2];
	for(i=0;i<bufferrx[2];i++)
	{
	Ansbuffer->Rxbuf[i]=bufferrx[3+i];
    }
	return(1);
}


/* --- EOF ------------------------------------------------------------------ */
