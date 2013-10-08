/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    ©2013 Semtech-Cycleo

Description:
	Configure Lora gateway and record received packets in a log file
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

/* fix an issue between POSIX and C99 */
#if __STDC_VERSION__ >= 199901L
	#define _XOPEN_SOURCE 600
#else
	#define _XOPEN_SOURCE 500
#endif

#include <stdint.h>		/* C99 types */
#include <stdbool.h>	/* bool type */
#include <stdio.h>		/* printf fprintf sprintf fopen fputs */

#include <string.h>		/* memset */
#include <signal.h>		/* sigaction */
#include <time.h>		/* time clock_gettime strftime gmtime clock_nanosleep*/
#include <unistd.h>		/* getopt */
#include <stdlib.h>		/* atoi */

#include "loragw_hal.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof((a)[0]))
#define MSG(args...)	fprintf(stderr,"loragw_pkt_logger: " args) /* message that is destined to the user */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define	LORANET_MIN_PKT_SIZE	16
#define	LORANET_MAC_OFFSET		4

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */

char str_buf[256]; /* temporary buffer to build and manipulate strings */
struct timespec sleep_time = {0, 3000000}; /* 3 ms */

/* signal handling variables */
struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
static int exit_sig = 0; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
static int quit_sig = 0; /* 1 -> application terminates without shutting down the hardware */

/* configuration file related */
char conf_file_name[64] = "loragw_conf.json"; /* default configuration file */
FILE * conf_file;

/* default configuration */
uint64_t lgwm = 0xAA555A0000000000; /* Lora gateway MAC address */
char lgwm_str[17];

/* clock, log file and log rotation management */
time_t now_time;
time_t log_start_time;
FILE * log_file = NULL;
int log_rotate_interval = 3600; /* by default, rotation every hour */
int time_check = 0; /* variable used to limit the number of calls to time() function */
char log_file_name[64];
unsigned long pkt_in_log; /* count the number of packet written in each log file */

/* allocate memory for packet fetching and processing */
struct lgw_pkt_rx_s rxpkt[16]; /* array containing up to 16 inbound packets metadata */
struct lgw_pkt_rx_s *p; /* pointer on a RX packet */
int nb_pkt;

/* variables for packet parsing and logging */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

static void sig_handler(int sigio);

void set_loragw_default_conf(void);

void open_log(void);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

static void sig_handler(int sigio) {
	if (sigio == SIGQUIT) {
		quit_sig = 1;;
	} else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
		exit_sig = 1;
	}
}

void set_loragw_default_conf(void) {
	int i;
	int err = 0;
	struct lgw_conf_rxrf_s rfconf;
	struct lgw_conf_rxif_s ifconf;
	
	memset(&rfconf, 0, sizeof(rfconf));
	memset(&ifconf, 0, sizeof(ifconf));
	
	/* set configuration for RF chains */
	rfconf.enable = true;
	rfconf.freq_hz = 866250000;
	i = lgw_rxrf_setconf(0, rfconf); /* radio A */
	if (i != LGW_HAL_SUCCESS) {
		MSG("WARNING: invalid configuration for radio A\n");
		++err;
	}
	
	rfconf.enable = true;
	rfconf.freq_hz = 866500000;
	i = lgw_rxrf_setconf(1, rfconf); /* radio B */
	if (i != LGW_HAL_SUCCESS) {
		MSG("WARNING: invalid configuration for radio B\n");
		++err;
	}
	
	/* set configuration for Lora multi-SF channels (bandwidth cannot be set) */
	
	ifconf.enable = true;
	ifconf.rf_chain = 0;
	ifconf.freq_hz = -187500;
	ifconf.datarate = DR_LORA_MULTI;
	i = lgw_rxif_setconf(0, ifconf); /* chain 0: Lora 125kHz, all SF, on 865.7 MHz */
	if (i != LGW_HAL_SUCCESS) {
		MSG("WARNING: invalid configuration for Lora multi-SF channel 0\n");
		++err;
	}
	
	ifconf.enable = true;
	ifconf.rf_chain = 0;
	ifconf.freq_hz = -62500;
	ifconf.datarate = DR_LORA_MULTI;
	i = lgw_rxif_setconf(1, ifconf); /* chain 1: Lora 125kHz, all SF, on 866.3 MHz */
	if (i != LGW_HAL_SUCCESS) {
		MSG("WARNING: invalid configuration for Lora multi-SF channel 1\n");
		++err;
	}
	
	ifconf.enable = true;
	ifconf.rf_chain = 0;
	ifconf.freq_hz = 62500;
	ifconf.datarate = DR_LORA_MULTI;
	i = lgw_rxif_setconf(2, ifconf); /* chain 2: Lora 125kHz, all SF, on 867.7 MHz */
	if (i != LGW_HAL_SUCCESS) {
		MSG("WARNING: invalid configuration for Lora multi-SF channel 2\n");
		++err;
	}
	
	ifconf.enable = true;
	ifconf.rf_chain = 1;
	ifconf.freq_hz = -62500;
	ifconf.datarate = DR_LORA_MULTI;
	i = lgw_rxif_setconf(3, ifconf); /* chain 3: Lora 125kHz, all SF, on 868.3 MHz */
	if (i != LGW_HAL_SUCCESS) {
		MSG("WARNING: invalid configuration for Lora multi-SF channel 3\n");
		++err;
	}
	
	/* set configuration for Lora 'stand alone' channel */
	ifconf.enable = true;
	ifconf.rf_chain = 1;
	ifconf.freq_hz = 125000;
	ifconf.bandwidth = BW_250KHZ;
	ifconf.datarate = DR_LORA_SF10;
	i = lgw_rxif_setconf(8, ifconf); /* chain 8: Lora 250kHz, SF10, on 866.0 MHz */
	if (i != LGW_HAL_SUCCESS) {
		MSG("WARNING: invalid configuration for Lora stand-alone channel\n");
		++err;
	}
	
	/* set configuration for FSK channel */
	ifconf.enable = true;
	ifconf.rf_chain = 1;
	ifconf.freq_hz = 125000;
	ifconf.bandwidth = BW_250KHZ;
	ifconf.datarate = 64000;
	i = lgw_rxif_setconf(9, ifconf); /* chain 9: FSK 64kbps, fdev 32kHz, variable payload, on 868.0 MHz */
	if (i != LGW_HAL_SUCCESS) {
		MSG("WARNING: invalid configuration for FSK channel\n");
		++err;
	}
	
	MSG("INFO: %d configuration error(s)\n", err);
	return;
}

void open_log(void) {
	int i;
	char iso_date[20];
	
	strftime(iso_date,ARRAY_SIZE(iso_date),"%Y%m%dT%H%M%SZ",gmtime(&now_time)); /* format yyyymmddThhmmssZ */
	log_start_time = now_time; /* keep track of when the log was started, for log rotation */
	
	sprintf(log_file_name, "pktlog_%s_%s.csv", lgwm_str, iso_date);
	log_file = fopen(log_file_name, "a"); /* create log file, append if file already exist */
	if (log_file == NULL) {
		MSG("ERROR: impossible to create log file %s\n", log_file_name);
		exit(EXIT_FAILURE);
	}
	
	i = fprintf(log_file, "\"gateway ID\",\"node MAC\",\"UTC timestamp\",\"us count\",\"frequency\",\"RF chain\",\"RX chain\",\"status\",\"size\",\"modulation\",\"bandwidth\",\"datarate\",\"coderate\",\"RSSI\",\"SNR\",\"payload\"\n");
	if (i < 0) {
		MSG("ERROR: impossible to write to log file %s\n", log_file_name);
		exit(EXIT_FAILURE);
	}
	
	MSG("INFO: Now writing to log file %s\n", log_file_name);
	pkt_in_log = 0;
	
	return;
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char **argv)
{
	int i, j, k; /* loop and temporary variables */
	
	/* local timestamp variables until we get accurate GPS time */
	struct timespec fetch_time;
	struct tm * x;
	char fetch_timestamp[30];
	
	/* parse command line options */
	while ((i = getopt (argc, argv, "hr:c:")) != -1) {
		switch (i) {
			case 'h':
				MSG( "Available options:\n");
				MSG( "-h print this help\n");
				MSG( "-r <int> rotate log file every N seconds (-1 disable log rotation)\n");
				MSG( "-c <filename> use specified configuration file instead of the default one\n");
				exit(EXIT_FAILURE);
			
			case 'r':
				log_rotate_interval = atoi(optarg);
				if ((log_rotate_interval == 0) || (log_rotate_interval < -1)) {
					MSG( "ERROR: Invalid argument for -r option\n");
					exit(EXIT_FAILURE);
				}
				break;
			
			case 'c':
				strcpy(conf_file_name, optarg);
				break;
			
			default:
				MSG("ERROR: argument parsing\n");
		}
	}
	
	/* configure signal handling */
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigact.sa_handler = sig_handler;
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	
	/* configuration data processing */
	sprintf(lgwm_str, "%016llX", lgwm);
	
	/* try to open and parse configuration file */
	conf_file = fopen(conf_file_name, "r");
	if(conf_file == NULL) {
		MSG("WARNING: impossible to open configuration file %s, applying default configuration\n", conf_file_name);
		set_loragw_default_conf();
	} else {
		/* try to parse configuration file */
		// TODO
		MSG("WARNING: failed to parse configuration file %s, applying default configuration\n", conf_file_name);
		set_loragw_default_conf();
	}
	
	/* starting the gateway */
	i = lgw_start();
	if (i == LGW_HAL_SUCCESS) {
		MSG("INFO: gateway started, packet can now be received\n");
	} else {
		MSG("ERROR: failed to start the gateway\n");
		exit(EXIT_FAILURE);
	}
	
	/* opening log file and writing CSV header*/
	time(&now_time);
	open_log();
	
	
	/* main loop */
	while ((quit_sig != 1) && (exit_sig != 1)) {
		/* fetch packets */
		nb_pkt = lgw_receive(ARRAY_SIZE(rxpkt), rxpkt);
		if (nb_pkt == LGW_HAL_ERROR) {
			MSG("ERROR: failed packet fetch, exiting\n");
			exit(EXIT_FAILURE);
		} else if (nb_pkt == 0) {
			clock_nanosleep(CLOCK_MONOTONIC, 0, &sleep_time, NULL); /* wait a short time if no packets */
		} else {
			/* local timestamp generation until we get accurate GPS time */
			clock_gettime(CLOCK_REALTIME, &fetch_time);
			x = gmtime(&(fetch_time.tv_sec));
			sprintf(fetch_timestamp,"%04i-%02i-%02i %02i:%02i:%02i.%03liZ",(x->tm_year)+1900,(x->tm_mon)+1,x->tm_mday,x->tm_hour,x->tm_min,x->tm_sec,(fetch_time.tv_nsec)/1000000); /* ISO 8601 format */
		}
		
		/* log packets */
		for (i=0; i < nb_pkt; ++i) {
			p = &rxpkt[i];
			
			/* writing gateway ID */
			fprintf(log_file, "%016llX,", lgwm);
			
			/* writing node MAC address */
			fputs("\"\",", log_file); // TODO: need to parse payload
			
			/* writing UTC timestamp*/
			fprintf(log_file, "\"%s\",", fetch_timestamp);
			// TODO: repalce with GPS time when available
			
			/* writing internal clock */
			fprintf(log_file, "%010u,", p->count_us);
			
			/* writing RX frequency */
			fputs("\"\",", log_file); // TODO: need updated HAL
			
			/* writing RF chain */
			fputs("\"\",", log_file); // TODO: need updated HAL
			
			/* writing RX modem/IF chain */
			fprintf(log_file, "%2d,", p->if_chain);
			
			/* writing status */
			switch(p->status) {
				case STAT_CRC_OK:	fputs("\"CRC_OK\" ,", log_file); break;
				case STAT_CRC_BAD:	fputs("\"CRC_BAD\",", log_file); break;
				case STAT_NO_CRC:	fputs("\"NO_CRC\" ,", log_file); break;
				case STAT_UNDEFINED:fputs("\"UNDEF\"  ,", log_file); break;
				default: fputs("\"ERR\"    ,", log_file);
			}
			
			/* writing payload size */
			fprintf(log_file, "%3u,", p->size);
			
			/* writing modulation */
			switch(p->modulation) {
				case MOD_LORA:	fputs("\"LORA\",", log_file); break;
				case MOD_FSK:	fputs("\"FSK\" ,", log_file); break;
				default: fputs("\"ERR\" ,", log_file);
			}
			
			/* writing bandwidth */
			switch(p->bandwidth) {
				case BW_500KHZ:	fputs("500000,", log_file); break;
				case BW_250KHZ:	fputs("250000,", log_file); break;
				case BW_125KHZ:	fputs("125000,", log_file); break;
				case BW_62K5HZ:	fputs("62500 ,", log_file); break;
				case BW_31K2HZ:	fputs("31200 ,", log_file); break;
				case BW_15K6HZ:	fputs("15600 ,", log_file); break;
				case BW_7K8HZ:	fputs("7800  ,", log_file); break;
				case BW_UNDEFINED: fputs("0     ,", log_file); break;
				default: fputs("-1    ,", log_file);
			}
			
			/* writing datarate */
			if (p->modulation == MOD_LORA) {
				switch (p->datarate) {
					case DR_LORA_SF7:	fputs("\"SF7\"   ,", log_file); break;
					case DR_LORA_SF8:	fputs("\"SF8\"   ,", log_file); break;
					case DR_LORA_SF9:	fputs("\"SF9\"   ,", log_file); break;
					case DR_LORA_SF10:	fputs("\"SF10\"  ,", log_file); break;
					case DR_LORA_SF11:	fputs("\"SF11\"  ,", log_file); break;
					case DR_LORA_SF12:	fputs("\"SF12\"  ,", log_file); break;
					default: fputs("\"ERR\"   ,", log_file);
				}
			} else if (p->modulation == MOD_FSK) {
				fprintf(log_file, "\"%6u\",", p->datarate);
			} else {
				fputs("\"ERR\"   ,", log_file);
			}
			
			/* writing coderate */
			switch (p->coderate) {
				case CR_LORA_4_5:	fputs("\"4/5\",", log_file); break;
				case CR_LORA_4_6:	fputs("\"2/3\",", log_file); break;
				case CR_LORA_4_7:	fputs("\"4/7\",", log_file); break;
				case CR_LORA_4_8:	fputs("\"1/2\",", log_file); break;
				case CR_UNDEFINED:	fputs("\"\"   ,", log_file); break;
				default: fputs("\"ERR\",", log_file);
			}
			
			/* writing packet RSSI */
			fprintf(log_file, "%+.0f,", p->rssi);
			
			/* writing packet average SNR */
			fprintf(log_file, "%+5.1f,", p->snr);
			
			/* writing hex-encoded payload (bundled in 32-bit words) */
			fputs("\"", log_file);
			for (j = 0; j < p->size; ++j) {
				if ((j > 0) && (j%4 == 0)) fputs("-", log_file);
				fprintf(log_file, "%02X", p->payload[j]);
			}
			fputs("\"\n", log_file);
			++pkt_in_log;
		}
		
		/* check time and rotate log file if necessary */
		++time_check;
		if (time_check >= 8) {
			time_check = 0;
			time(&now_time);
			if (difftime(now_time, log_start_time) > log_rotate_interval) {
				fclose(log_file);
				MSG("INFO: log file %s closed, %lu packet(s) recorded\n", log_file_name, pkt_in_log);
				open_log();
			}
		}
	}
	
	if (exit_sig == 1) {
		/* clean up before leaving */
		i = lgw_stop();
		if (i == LGW_HAL_SUCCESS) {
			MSG("INFO: gateway stopped successfully\n");
		} else {
			MSG("WARNING: failed to stop gateway successfully\n");
		}
		fclose(log_file);
		MSG("INFO: log file %s closed, %lu packet(s) recorded\n", log_file_name, pkt_in_log);
	}
	
	printf("\nEnd of test for loragw_hal.c\n");
	
	return EXIT_SUCCESS;
}

/* --- EOF ------------------------------------------------------------------ */
