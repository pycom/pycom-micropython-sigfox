/* 
* Copyright (c) 2017, Pycom Limited and its licensors.
*
* This software is licensed under the GNU GPL version 3 or any later version,
* with permitted additional terms. For more information see the Pycom Licence
* v1.0 document supplied with this file, or available at:
* https://www.pycom.io/opensource/licensing
*
* This file contains code under the following copyright and licensing notices.
* The code has been changed but otherwise retained.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "tcpip_adapter.h"
#include "netif/ppp/pppos.h"
#include "netif/ppp/ppp.h"
#include "lwip/pppapi.h"

#include "lib3GPP.h"
#include "mpconfigboard.h"

#define CONFIG_GSM_BDRATE 921600
#define CONFIG_GSM_INTERNET_USER ""
#define CONFIG_GSM_INTERNET_PASSWORD ""
#define CONFIG_GSM_APN "internet"

#define UART_GPIO_TX CONFIG_GSM_TX
#define UART_GPIO_RX CONFIG_GSM_RX
#define UART_PIN_CTS CONFIG_GSM_CTS
#define UART_PIN_RTS CONFIG_GSM_RTS
#define UART_BDRATE CONFIG_GSM_BDRATE
#define GSM_DEBUG 0

#define BUF_SIZE (1024)
#define GSM_OK_Str "OK"
#define PPPOSMUTEX_TIMEOUT 1000 / portTICK_RATE_MS

#define PPPOS_CLIENT_STACK_SIZE 1024*3


// shared variables, use mutex to access them
static uint8_t gsm_status = GSM_STATE_FIRSTINIT;
static int do_pppos_connect = 1;
static uint32_t pppos_rx_count;
static uint32_t pppos_tx_count;
static uint8_t pppos_task_started = 0;
static uint8_t gsm_rfOff = 0;

// local variables
static QueueHandle_t pppos_mutex = NULL;
const char *PPP_User = CONFIG_GSM_INTERNET_USER;
const char *PPP_Pass = CONFIG_GSM_INTERNET_PASSWORD;
static int uart_num = UART_NUM_2;

static uint8_t tcpip_adapter_initialized = 0;

// The PPP control block
static ppp_pcb *ppp = NULL;

// The PPP IP interface
struct netif ppp_netif;

typedef struct
{
	char		*cmd;
	uint16_t	cmdSize;
	char		*cmdResponseOnOk;
	uint16_t	timeoutMs;
	uint16_t	delayMs;
	uint8_t		skip;
} GSM_Cmd;

typedef struct
{
} GSM_obj;


static GSM_Cmd cmd_AT =
{
	.cmd = "AT\r\n",
	.cmdSize = sizeof("AT\r\n")-1,
	.cmdResponseOnOk = GSM_OK_Str,
	.timeoutMs = 500,
	.delayMs = 0,
	.skip = 0,
};

static GSM_Cmd cmd_HardReset =
{
	.cmd = "AT^RESET\r\n",
	.cmdSize = sizeof("AT^RESET\r\n")-1,
	.cmdResponseOnOk = GSM_OK_Str,
	.timeoutMs = 500,
	.delayMs = 0,
	.skip = 0,
};

static GSM_Cmd cmd_Reset =
{
	.cmd = "ATZ\r\n",
	.cmdSize = sizeof("ATZ\r\n")-1,
	.cmdResponseOnOk = GSM_OK_Str,
	.timeoutMs = 300,
	.delayMs = 0,
	.skip = 0,
};

static GSM_Cmd cmd_Freq =
{
	.cmd = "AT!=\"addscanfreq band=28 dl-earfcn=9435\"\r\n",
	.cmdSize = sizeof("AT!=\"addscanfreq band=28 dl-earfcn=9435\"\r\n")-1,
	.cmdResponseOnOk = GSM_OK_Str,
	.timeoutMs = 10000,
	.delayMs = 1000,
	.skip = 0,
};


static GSM_Cmd cmd_RFOn =
{
	.cmd = "AT+CFUN=1\r\n",
	.cmdSize = sizeof("ATCFUN=1,0\r\n")-1,
	.cmdResponseOnOk = GSM_OK_Str,
	.timeoutMs = 10000,
	.delayMs = 1000,
	.skip = 0,
};

static GSM_Cmd cmd_EchoOff =
{
	.cmd = "ATE0\r\n",
	.cmdSize = sizeof("ATE0\r\n")-1,
	.cmdResponseOnOk = GSM_OK_Str,
	.timeoutMs = 300,
	.delayMs = 0,
	.skip = 0,
};


static GSM_Cmd cmd_PowerSave =
{
	.cmd = "AT+CPSMS=1\r\n",
	.cmdSize = sizeof("AT+CPSMS=1\r\n")-1,
	.cmdResponseOnOk = GSM_OK_Str,
	.timeoutMs = 300,
	.delayMs = 0,
	.skip = 0,
};



static GSM_Cmd cmd_Pin =
{
	.cmd = "AT+CPIN?\r\n",
	.cmdSize = sizeof("AT+CPIN?\r\n")-1,
	.cmdResponseOnOk = "CPIN: READY",
	.timeoutMs = 5000,
	.delayMs = 0,
	.skip = 0,
};

static GSM_Cmd cmd_Reg =
{
	.cmd = "AT+CEREG?\r\n",
	.cmdSize = sizeof("AT+CEREG?\r\n")-1,
	.cmdResponseOnOk = "+CEREG: 2,1",
	.timeoutMs = 3000,
	.delayMs = 2000,
	.skip = 0,
};

static GSM_Cmd cmd_CMEE =
{
	.cmd = "AT+CMEE=2\r\n",
	.cmdSize = sizeof("AT+CMEE=2\r\n")-1,
	.cmdResponseOnOk = GSM_OK_Str,
	.timeoutMs = 1000,
	.delayMs = 0,
	.skip = 0,
};

static GSM_Cmd cmd_COPS =
{
	.cmd = "AT+COPS=0\r\n",
	.cmdSize = sizeof("AT+COPS=0\r\n")-1,
	.cmdResponseOnOk = GSM_OK_Str,
	.timeoutMs = 1000,
	.delayMs = 0,
	.skip = 0,
};

static GSM_Cmd cmd_APN =
{
	.cmd = NULL,
	.cmdSize = 0,
	.cmdResponseOnOk = GSM_OK_Str,
	.timeoutMs = 8000,
	.delayMs = 0,
	.skip = 0,
};

static GSM_Cmd cmd_Connect =
{
	.cmd = "AT+CGDATA=\"PPP\",1\r\n",
	.cmdSize = sizeof("AT+CGDATA=\"PPP\",1\r\n")-1,
	.cmdResponseOnOk = "CONNECT",
	.timeoutMs = 30000,
	.delayMs = 1000,
	.skip = 0,
};

static GSM_Cmd *GSM_Init[] =
{
		&cmd_AT,
		&cmd_AT,
		&cmd_Reset,
		&cmd_CMEE,
		&cmd_PowerSave,
		&cmd_RFOn,
		&cmd_Reg,
		&cmd_Connect,
};

#define GSM_InitCmdsSize  (sizeof(GSM_Init)/sizeof(GSM_Cmd *))


// PPP status callback
//--------------------------------------------------------------
static void ppp_status_cb(ppp_pcb *pcb, int err_code, void *ctx)
{
	struct netif *pppif = ppp_netif(pcb);
	LWIP_UNUSED_ARG(ctx);

	switch(err_code) {
		case PPPERR_NONE: {
			#if GSM_DEBUG
			printf("status_cb: Connected");
			#if PPP_IPV4_SUPPORT
			printf("ipaddr    = %s", ipaddr_ntoa(&pppif->ip_addr));
			printf("gateway   = %s", ipaddr_ntoa(&pppif->gw));
			printf("netmask   = %s", ipaddr_ntoa(&pppif->netmask));
			#endif

			#if PPP_IPV6_SUPPORT
			printf("ip6addr   = %s", ip6addr_ntoa(netif_ip6_addr(pppif, 0)));
			#endif
			#endif
			xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
			gsm_status = GSM_STATE_CONNECTED;
			xSemaphoreGive(pppos_mutex);
			break;
		}
		case PPPERR_PARAM: {
			#if GSM_DEBUG
			printf("status_cb: Invalid parameter");
			#endif
			break;
		}
		case PPPERR_OPEN: {
			#if GSM_DEBUG
			printf("status_cb: Unable to open PPP session");
			#endif
			break;
		}
		case PPPERR_DEVICE: {
			#if GSM_DEBUG
			printf("status_cb: Invalid I/O device for PPP");
			#endif
			break;
		}
		case PPPERR_ALLOC: {
			#if GSM_DEBUG
			printf("status_cb: Unable to allocate resources");
			#endif
			break;
		}
		case PPPERR_USER: {
			/* ppp_free(); -- can be called here */
			#if GSM_DEBUG
			printf("status_cb: User interrupt (disconnected)");
			#endif
			xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
			gsm_status = GSM_STATE_DISCONNECTED;
			xSemaphoreGive(pppos_mutex);
			break;
		}
		case PPPERR_CONNECT: {
			#if GSM_DEBUG
			printf("status_cb: Connection lost");
			#endif
			xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
			gsm_status = GSM_STATE_DISCONNECTED;
			xSemaphoreGive(pppos_mutex);
			break;
		}
		case PPPERR_AUTHFAIL: {
			#if GSM_DEBUG
			printf("status_cb: Failed authentication challenge");
			#endif
			break;
		}
		case PPPERR_PROTOCOL: {
			#if GSM_DEBUG
			printf("status_cb: Failed to meet protocol");
			#endif
			break;
		}
		case PPPERR_PEERDEAD: {
			#if GSM_DEBUG
			printf("status_cb: Connection timeout");
			#endif
			break;
		}
		case PPPERR_IDLETIMEOUT: {
			#if GSM_DEBUG
			printf("status_cb: Idle Timeout");
			#endif
			break;
		}
		case PPPERR_CONNECTTIME: {
			#if GSM_DEBUG
			printf("status_cb: Max connect time reached");
			#endif
			break;
		}
		case PPPERR_LOOPBACK: {
			#if GSM_DEBUG
			printf("status_cb: Loopback detected");
			#endif
			break;
		}
		default: {
			#if GSM_DEBUG
			printf("status_cb: Unknown error code %d", err_code);
			#endif
			break;
		}
	}
}

// === Handle sending data to GSM modem ===
//------------------------------------------------------------------------------
static u32_t ppp_output_callback(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx)
{
	uint32_t ret = uart_write_bytes(uart_num, (const char*)data, len);
    uart_wait_tx_done(uart_num, 10 / portTICK_RATE_MS);
    if (ret > 0) {
		xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
    	pppos_rx_count += ret;
		xSemaphoreGive(pppos_mutex);
    }
    return ret;
}

//---------------------------------------------------------
static void infoCommand(char *cmd, int cmdSize, char *info)
{
	char buf[cmdSize+2];
	memset(buf, 0, cmdSize+2);

	for (int i=0; i<cmdSize;i++) {
		if ((cmd[i] != 0x00) && ((cmd[i] < 0x20) || (cmd[i] > 0x7F))) buf[i] = '.';
		else buf[i] = cmd[i];
		if (buf[i] == '\0') break;
	}
	printf("%s [%s]", info, buf);
}

//----------------------------------------------------------------------------------------------------------------------
static int atCmd_waitResponse(char * cmd, char *resp, char * resp1, int cmdSize, int timeout, char **response, int size)
{
	char sresp[256] = {'\0'};
	char data[256] = {'\0'};
    int len, res = 1, idx = 0, tot = 0, timeoutCnt = 0;

	// ** Send command to GSM
	vTaskDelay(100 / portTICK_PERIOD_MS);
	uart_flush(uart_num);

	if (cmd != NULL) {
		if (cmdSize == -1) cmdSize = strlen(cmd);
		#if GSM_DEBUG
		infoCommand(cmd, cmdSize, "AT COMMAND:");
		#endif
		uart_write_bytes(uart_num, (const char*)cmd, cmdSize);
		uart_wait_tx_done(uart_num, 100 / portTICK_RATE_MS);
	}

	if (response != NULL) {
		// Read GSM response into buffer
		char *pbuf = *response;
		len = uart_read_bytes(uart_num, (uint8_t*)data, 256, timeout / portTICK_RATE_MS);
		while (len > 0) {
			if ((tot+len) >= size) {
				char *ptemp = realloc(pbuf, size+512);
				if (ptemp == NULL) return 0;
				size += 512;
				pbuf = ptemp;
			}
			memcpy(pbuf+tot, data, len);
			tot += len;
			response[tot] = '\0';
			len = uart_read_bytes(uart_num, (uint8_t*)data, 256, 100 / portTICK_RATE_MS);
		}
		*response = pbuf;
		return tot;
	}

    // ** Wait for and check the response
	idx = 0;
	while(1)
	{
		memset(data, 0, 256);
		len = 0;
		len = uart_read_bytes(uart_num, (uint8_t*)data, 256, 10 / portTICK_RATE_MS);
		if (len > 0) {
			for (int i=0; i<len;i++) {
				if (idx < 256) {
					if ((data[i] >= 0x20) && (data[i] < 0x80)) sresp[idx++] = data[i];
					else sresp[idx++] = 0x2e;
				}
			}
			tot += len;
		}
		else {
			if (tot > 0) {
				// Check the response
				if (strstr(sresp, resp) != NULL) {
					#if GSM_DEBUG
					printf("AT RESPONSE: [%s]", sresp);
					#endif
					break;
				}
				else {
					if (resp1 != NULL) {
						if (strstr(sresp, resp1) != NULL) {
							#if GSM_DEBUG
							printf("AT RESPONSE (1): [%s]", sresp);
							#endif
							res = 2;
							break;
						}
					}
					// no match
					#if GSM_DEBUG
					printf("AT BAD RESPONSE: [%s]", sresp);
					#endif
					res = 0;
					break;
				}
			}
		}

		timeoutCnt += 10;
		if (timeoutCnt > timeout) {
			// timeout
			#if GSM_DEBUG
			printf("AT: TIMEOUT");
			#endif
			res = 0;
			break;
		}
	}

	return res;
}

//------------------------------------
static void _disconnect(uint8_t rfOff)
{
	int res = atCmd_waitResponse("AT\r\n", GSM_OK_Str, NULL, 4, 1000, NULL, 0);
	if (res == 1) {
		if (rfOff) {
			cmd_Reg.timeoutMs = 10000;
			res = atCmd_waitResponse("AT+CFUN=4\r\n", GSM_OK_Str, NULL, 11, 10000, NULL, 0); // disable RF function
		}
		return;
	}

	#if GSM_DEBUG
	printf("ONLINE, DISCONNECTING...");
	#endif
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	uart_flush(uart_num);
	uart_write_bytes(uart_num, "+++", 3);
    uart_wait_tx_done(uart_num, 10 / portTICK_RATE_MS);
	vTaskDelay(1100 / portTICK_PERIOD_MS);

	int n = 0;
	res = atCmd_waitResponse("ATH\r\n", GSM_OK_Str, "NO CARRIER", 5, 3000, NULL, 0);
	while (res == 0) {
		n++;
		if (n > 3) {
			#if GSM_DEBUG
			printf("STILL CONNECTED.");
			#endif
			n = 0;
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			uart_flush(uart_num);
			uart_write_bytes(uart_num, "+++", 3);
		    uart_wait_tx_done(uart_num, 10 / portTICK_RATE_MS);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}
		vTaskDelay(100 / portTICK_PERIOD_MS);
		res = atCmd_waitResponse("ATH\r\n", GSM_OK_Str, "NO CARRIER", 5, 3000, NULL, 0);
	}
	vTaskDelay(100 / portTICK_PERIOD_MS);
	if (rfOff) {
		cmd_Reg.timeoutMs = 10000;
		res = atCmd_waitResponse("AT+CFUN=4\r\n", GSM_OK_Str, NULL, 11, 3000, NULL, 0);
	}
	#if GSM_DEBUG
	printf("DISCONNECTED.");
	#endif
}

//----------------------------
static void enableAllInitCmd()
{
	for (int idx = 0; idx < GSM_InitCmdsSize; idx++) {
		GSM_Init[idx]->skip = 0;
	}
}

/*
 * PPPoS TASK
 * Handles GSM initialization, disconnects and GSM modem responses
 */
//-----------------------------
static void pppos_client_task()
{
	xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
	pppos_task_started = 1;
	xSemaphoreGive(pppos_mutex);

    // Allocate receive buffer
    char* data = (char*) malloc(BUF_SIZE);
    if (data == NULL) {
		#if GSM_DEBUG
		printf("3GPP: Failed to allocate data buffer.");
		#endif
    	goto exit;
    }

    if (gpio_set_direction(UART_GPIO_TX, GPIO_MODE_OUTPUT)) goto exit;
	if (gpio_set_direction(UART_GPIO_RX, GPIO_MODE_INPUT)) goto exit;
	if (gpio_set_direction(UART_PIN_CTS, GPIO_MODE_INPUT)) goto exit;
	if (gpio_set_direction(UART_PIN_RTS, GPIO_MODE_OUTPUT)) goto exit;
	if (gpio_set_pull_mode(UART_GPIO_RX, GPIO_PULLUP_ONLY)) goto exit;

	#if GSM_DEBUG
	printf("GPIO direction configured\n");
	#endif

	char PPP_ApnATReq[sizeof(CONFIG_GSM_APN)+24];
	
	
	uart_config_t uart_config = {
			.baud_rate = UART_BDRATE,
			.data_bits = UART_DATA_8_BITS,
			.parity = UART_PARITY_DISABLE,
			.stop_bits = UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS
	};

	//Configure UART1 parameters
	if (uart_param_config(uart_num, &uart_config)) goto exit;
	
	//Set UART1 pins(TX, RX, RTS, CTS)
	if (uart_set_pin(uart_num, UART_GPIO_TX, UART_GPIO_RX, UART_PIN_RTS, UART_PIN_CTS)) goto exit;
	#if GSM_DEBUG	
	printf("uart_set_pin done \n");
	#endif
    
	if (uart_driver_install(uart_num, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0, NULL)) goto exit; 
	#if GSM_DEBUG	
	printf("uart_driver_install done \n");
	#endif

	// Set APN from config
	sprintf(PPP_ApnATReq, "AT+CGDCONT=1,\"IP\",\"%s\"\r\n", CONFIG_GSM_APN);
	cmd_APN.cmd = PPP_ApnATReq;
	cmd_APN.cmdSize = strlen(PPP_ApnATReq);

	_disconnect(1); // Disconnect if connected

	xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
    pppos_tx_count = 0;
    pppos_rx_count = 0;
	gsm_status = GSM_STATE_FIRSTINIT;
	xSemaphoreGive(pppos_mutex);

	enableAllInitCmd();

	while(1)
	{
		#if GSM_DEBUG
		printf("GSM initialization start");
		#endif
		vTaskDelay(500 / portTICK_PERIOD_MS);

    	if (do_pppos_connect <= 0) {
    	    cmd_RFOn.skip = 1; 
		    cmd_Reg.skip = 1;
			cmd_Connect.skip = 1;
			cmd_APN.skip = 1;
		}
		int gsmCmdIter = 0;
		int nfail = 0;
		
		// * GSM Initialization loop
		while(gsmCmdIter < GSM_InitCmdsSize)
		{
			if (GSM_Init[gsmCmdIter]->skip) {
				gsmCmdIter++;
				continue;
			}
			if (atCmd_waitResponse(GSM_Init[gsmCmdIter]->cmd,
					GSM_Init[gsmCmdIter]->cmdResponseOnOk, NULL,
					GSM_Init[gsmCmdIter]->cmdSize,
					GSM_Init[gsmCmdIter]->timeoutMs, NULL, 0) == 0)
			{
				// * No response or not as expected, start from first initialization command
				#if GSM_DEBUG
				printf("C, restarting...");
				#endif
            
				nfail++;
				if (nfail > 20) goto exit;

				vTaskDelay(3000 / portTICK_PERIOD_MS);
				gsmCmdIter = 0;
				continue;
			}

			if (GSM_Init[gsmCmdIter]->delayMs > 0) vTaskDelay(GSM_Init[gsmCmdIter]->delayMs / portTICK_PERIOD_MS);
			GSM_Init[gsmCmdIter]->skip = 1;
			if (GSM_Init[gsmCmdIter] == &cmd_Reg) GSM_Init[gsmCmdIter]->delayMs = 0;
			// Next command
			gsmCmdIter++;
		}

		#if GSM_DEBUG
		printf("GSM initialized.");
		#endif

		xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
		if (gsm_status == GSM_STATE_FIRSTINIT) {
			xSemaphoreGive(pppos_mutex);
			// ** After first successful initialization create PPP control block
			ppp = pppapi_pppos_create(&ppp_netif,
					ppp_output_callback, ppp_status_cb, NULL);

			if (ppp == NULL) {
				#if GSM_DEBUG
				printf("Error initializing PPPoS");
				#endif
				break; // end task
			}
			//netif_set_default(&ppp_netif);
		} else {
			gsm_status = GSM_STATE_IDLE;
			xSemaphoreGive(pppos_mutex);
		}


        int gstat = 0;
        if (do_pppos_connect <= 0) {
            #if GSM_DEBUG
			printf("PPPoS IDLE mode");
            #endif
            
            xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
            gsm_status = GSM_STATE_IDLE;
            xSemaphoreGive(pppos_mutex);
            
            // === Wait for connect request ===
			gstat = 0;
			while (gstat == 0) {
				vTaskDelay(100 / portTICK_PERIOD_MS);
				xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
				gstat = do_pppos_connect;
				xSemaphoreGive(pppos_mutex);
			}
			if (gstat < 0) break;  // terminate task
			gsmCmdIter = 0;
            enableAllInitCmd();
            cmd_Connect.skip = 0;
            cmd_APN.skip = 0;
            xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
            do_pppos_connect = 1;
            xSemaphoreGive(pppos_mutex); 
			#if GSM_DEBUG
				printf("Connect requested.");
			#endif
			continue;
		}		
		if (gstat < 0) break;  // terminate task
		
		
		pppapi_set_default(ppp);
		pppapi_set_auth(ppp, PPPAUTHTYPE_PAP, PPP_User, PPP_Pass);
		//pppapi_set_auth(ppp, PPPAUTHTYPE_NONE, PPP_User, PPP_Pass);

		pppapi_connect(ppp, 0);
        gstat = 1;
        
		// *** LOOP: Handle GSM modem responses & disconnects ***
		while(1) {
			// === Check if disconnect requested ===
			xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
			if (do_pppos_connect <= 0) {
				int end_task = do_pppos_connect;
				do_pppos_connect = 1;
				xSemaphoreGive(pppos_mutex);
				#if GSM_DEBUG
				printf("Disconnect requested.");
				#endif

				pppapi_close(ppp, 0);
				int gstat = 1;
				while (gsm_status != GSM_STATE_DISCONNECTED) {
					// Handle data received from GSM
					memset(data, 0, BUF_SIZE);
					int len = uart_read_bytes(uart_num, (uint8_t*)data, BUF_SIZE, 30 / portTICK_RATE_MS);
					if (len > 0)	{
						pppos_input_tcpip(ppp, (u8_t*)data, len);
						xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
					    pppos_tx_count += len;
						xSemaphoreGive(pppos_mutex);
					}
					xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
					gstat = gsm_status;
					xSemaphoreGive(pppos_mutex);
				}
				vTaskDelay(1000 / portTICK_PERIOD_MS);

				xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
				uint8_t rfoff = gsm_rfOff;
				xSemaphoreGive(pppos_mutex);
				_disconnect(rfoff); // Disconnect GSM if still connected

				#if GSM_DEBUG
				printf("Disconnected.");
				#endif

				gsmCmdIter = 0;
				enableAllInitCmd();
				xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
				gsm_status = GSM_STATE_IDLE;
				do_pppos_connect = 0;
				xSemaphoreGive(pppos_mutex);

				if (end_task < 0) goto exit;

				// === Wait for reconnect request ===
				gstat = 0;
				while (gstat == 0) {
					vTaskDelay(100 / portTICK_PERIOD_MS);
					xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
					gstat = do_pppos_connect;
					xSemaphoreGive(pppos_mutex);
				}
				if (gstat < 0) break;  // terminate task
				#if GSM_DEBUG
				printf("Reconnect requested.");
				#endif
				break;
			}
			
			// === Check if disconnected ===
			if (gsm_status == GSM_STATE_DISCONNECTED) {
			    gsm_status = GSM_STATE_IDLE;
				xSemaphoreGive(pppos_mutex);
				#if GSM_DEBUG
				printf("Disconnected, trying again...");
				#endif
				pppapi_close(ppp, 0);

				enableAllInitCmd();
				gsmCmdIter = 0;
				vTaskDelay(5000 / portTICK_PERIOD_MS);
				break;
			}
			else xSemaphoreGive(pppos_mutex);

			// === Handle data received from GSM ===
			memset(data, 0, BUF_SIZE);
			int len = uart_read_bytes(uart_num, (uint8_t*)data, BUF_SIZE, 30 / portTICK_RATE_MS);
			if (len > 0)	{
				pppos_input_tcpip(ppp, (u8_t*)data, len);
				xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
			    pppos_tx_count += len;
				xSemaphoreGive(pppos_mutex);
			}

		}  // Handle GSM modem responses & disconnects loop
		if (gstat < 0) break;  // terminate task
	}  // main task loop

exit:
    xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
	if (data) free(data);  // free data buffer
	if (ppp) ppp_free(ppp);


	pppos_task_started = 0;
	gsm_status = GSM_STATE_FIRSTINIT;
	uart_driver_delete(uart_num);
	xSemaphoreGive(pppos_mutex);
	#if GSM_DEBUG
	printf("PPPoS TASK TERMINATED");
	#endif
	vTaskDelete(NULL);
}

//=============
int ppposInit()
{
	if (pppos_mutex != NULL) xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
	do_pppos_connect = 0;

	int task_s = pppos_task_started;
	if (pppos_mutex != NULL) xSemaphoreGive(pppos_mutex);

	if (task_s == 0) {
		if (pppos_mutex == NULL) pppos_mutex = xSemaphoreCreateMutex();
		if (pppos_mutex == NULL) return 0;

		if (tcpip_adapter_initialized == 0) {
			tcpip_adapter_init();
			tcpip_adapter_initialized = 1;
		}
		xTaskCreate(&pppos_client_task, "pppos_client_task", PPPOS_CLIENT_STACK_SIZE, NULL, 10, NULL);
		while (task_s == 0) {
			vTaskDelay(10 / portTICK_RATE_MS);
			xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
			task_s = pppos_task_started;
			xSemaphoreGive(pppos_mutex);
		}
	} else {
	
	    // PPPoS task already running	    		
	    if (tcpip_adapter_initialized == 0) {
			tcpip_adapter_init();
			tcpip_adapter_initialized = 1;
		}
	
	}
	return 0;
}

//=============
int ppposConnect()
{
    if (pppos_mutex != NULL) xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
	do_pppos_connect = 1;
	int gstat = 0;
	int task_s = pppos_task_started;
	if (pppos_mutex != NULL) xSemaphoreGive(pppos_mutex);

    // Hold until connection is obtained TODO make it as "blocking" flag
	while (gstat != 1) {
		vTaskDelay(10 / portTICK_RATE_MS);
		xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
		gstat = gsm_status;
		task_s = pppos_task_started;
		xSemaphoreGive(pppos_mutex);
		if (task_s == 0) return 0;
	}
	
	return 0;
}

//===================================================
void ppposDisconnect(uint8_t end_task, uint8_t rfoff)
{

    #if GSM_DEBUG
        printf("ppposDisconnect called");
    #endif


    if (pppos_mutex == NULL) return;
    
	xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
	int gstat = gsm_status;
	int task_s = pppos_task_started;
	xSemaphoreGive(pppos_mutex);
	
	if (task_s == 0) return;

    #if GSM_DEBUG
        printf("task_s != 0");
    #endif

	if ((gstat == GSM_STATE_IDLE) && (end_task == 0)) return; 

	vTaskDelay(2000 / portTICK_RATE_MS);
	xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
	if (end_task) do_pppos_connect = -1;
	else do_pppos_connect = 0;
	gsm_rfOff = rfoff;
	xSemaphoreGive(pppos_mutex);

	gstat = 0;
	while ((gstat == 0) && (task_s != 0)) {
		xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
		gstat = do_pppos_connect;
		task_s = pppos_task_started;
		xSemaphoreGive(pppos_mutex);
		vTaskDelay(10 / portTICK_RATE_MS);
	}
	if (task_s == 0) return;
	
	while ((gstat != 0) && (task_s != 0)) {
		vTaskDelay(100 / portTICK_RATE_MS);
		xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
		gstat = do_pppos_connect;
		task_s = pppos_task_started;
		xSemaphoreGive(pppos_mutex);
	}
}

//===================
int ppposStatus()
{
	xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
	int gstat = gsm_status;
	xSemaphoreGive(pppos_mutex);

	return gstat;
}

//========================================================
void getRxTxCount(uint32_t *rx, uint32_t *tx, uint8_t rst)
{
	xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
	*rx = pppos_rx_count;
	*tx = pppos_tx_count;
	if (rst) {
		pppos_rx_count = 0;
		pppos_tx_count = 0;
	}
	xSemaphoreGive(pppos_mutex);
}

//===================
void resetRxTxCount()
{
	xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
	pppos_rx_count = 0;
	pppos_tx_count = 0;
	xSemaphoreGive(pppos_mutex);
}

//=============
int gsm_RFOff()
{
	xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
	int gstat = gsm_status;
	xSemaphoreGive(pppos_mutex);

	if (gstat != GSM_STATE_IDLE) return 0;

	uint8_t f = 1;
	char buf[64] = {'\0'};
	char *pbuf = buf;
	int res = atCmd_waitResponse("AT+CFUN?\r\n", NULL, NULL, -1, 2000, &pbuf, 63);
	if (res > 0) {
		if (strstr(buf, "+CFUN: 4")) f = 0;
	}

	if (f) {
		cmd_Reg.timeoutMs = 500;
		return atCmd_waitResponse("AT+CFUN=4\r\n", GSM_OK_Str, NULL, 11, 10000, NULL, 0); // disable RF function
	}
	return 1;
}

//============
int gsm_RFOn()
{
	xSemaphoreTake(pppos_mutex, PPPOSMUTEX_TIMEOUT);
	int gstat = gsm_status;
	xSemaphoreGive(pppos_mutex);

	if (gstat != GSM_STATE_IDLE) return 0;

	uint8_t f = 1;
	char buf[64] = {'\0'};
	char *pbuf = buf;
	int res = atCmd_waitResponse("AT+CFUN?\r\n", NULL, NULL, -1, 2000, &pbuf, 63);
	if (res > 0) {
		if (strstr(buf, "+CFUN: 1")) f = 0;
	}

	if (f) {
		cmd_Reg.timeoutMs = 0;
		return atCmd_waitResponse("AT+CFUN=1\r\n", GSM_OK_Str, NULL, 11, 10000, NULL, 0); // disable RF function
	}
	return 1;
}