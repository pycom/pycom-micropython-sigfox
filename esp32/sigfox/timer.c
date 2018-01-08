//*****************************************************************************
//! @file       timer.c
//! @brief      Timer Management.
//!				Two timers are used in this project.
//!       \li \e Timer1 is used to generate bit rate ( fcc : 600bps / etsi : 100bps )
//!       \li \e Timer0 is used to ensure SigFox Downlink protocol timings are under control
//!              \li \c 20 s Waiting time after the 1st INITIATE_DOWNLINK Uplink Frame
//!              \li \c 25 s Reception windows to get the Downling frame
//!
//****************************************************************************/


/**************************************************************************//**
* @addtogroup Timer
* @{
******************************************************************************/


/******************************************************************************
 * INCLUDES
 */
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "py/mpconfig.h"
#include "sigfox_api.h"
#include "targets/hal_spi_rf_trxeb.h"
#include "manufacturer_api.h"
#include "sigfox/sigfox_types.h"
#include "transmission.h"
#include "targets/cc112x_spi.h"
#if defined (FIPY) || defined (LOPY)
#include "radio_sx127x.h"      // for packetSemaphore
#else
#include "radio.h"      // for packetSemaphore
#endif

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_attr.h"
#include "gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/xtensa_api.h"


/******************************************************************************
 * GLOBAL VARIABLES
 */
extern sfx_u8 uplink_spectrum_access;

/******************************************************************************
 * DEFINES
 */
typedef enum {
    E_TIMER_MODE_MODULATION = 0,
    E_TIMER_MODE_RSSI
} e_timer_mode;

/******************************************************************************
 * LOCAL VARIABLES
 */
extern e_SystemState SysState;

bool TIMER_downlink_timeout;
bool TIMER_carrier_sense_timeout;
bool TIMER_rssi_end;
#if !defined(FIPY) && !defined(LOPY4)
static bool rxtx_in_progress;
#endif
static sfx_u32   TIMER_bitrate_interrupt_count;
static sfx_u32   TIMER_downlink_interrupt_count;
static sfx_u32   TIMER_carrier_sense_interrupt_count;
static sfx_u32   TIMER_bitrate_nb_interrupt_to_wait_for;
static sfx_u32   TIMER_downlink_nb_interrupt_to_wait_for;
static sfx_u32   TIMER_carrier_sense_nb_interrupt_to_wait_for;
static e_timer_mode TIMER_bitrate_mode;
static uint32_t TIMER_bitrate_ticks;
static TimerHandle_t TIMER_downlink;
static TimerHandle_t TIMER_clear_channel;
#if !defined(FIPY) && !defined(LOPY4)
static TimerHandle_t TIMER_RxTx_done;
#endif

#define TIMER_BITRATE_NUM                   1

#define FCC_TIMER_TICKS                     (133120 * 2)       // 1 bit each 1.66 ms @160MHz
#define ETSI_TIMER_TICKS                    (800000 * 2)       // 1 bit each 10ms @160MHz
#define RSSI_TIMER_TICKS                    (20000 * 2)        // every 250us
#define CARRIER_SENSE_TIMER_TICKS           (80000 * 2)        // every 1ms

#define RSSI_TIMER_US                       (250)

IRAM_ATTR void __delay_cycles(unsigned int volatile x) {
    while(--x);
}

/******************************************************************************
 * FUNCTIONS
 */
/**************************************************************************//**
*   @brief 		Initialize the Timer uses to produce the signal bitrate
*          \li 	FCC bit rate  : 600bps => 1 bit each 1.66 ms
*          \li 	ETSI bit rate : 100bps => 1 bit each 10 ms
******************************************************************************/

/***************************************************************************//**
*   @brief  Timer1 interrupt : a new bit has to be sent to the Radio
*           Change the system state status to processing
*******************************************************************************/
IRAM_ATTR void TIMER_bitrate_isr (void* para) {
    // sfx_u8 rssi1;
    // sfx_s8 rssi2;
    // sfx_u8 rssi0;
    // sfx_s16 rssi;
    // sfx_s16 rssiConverted;
    // sfx_s16 rssi2Converted;
    xthal_set_ccompare(TIMER_BITRATE_NUM, xthal_get_ccount() + TIMER_bitrate_ticks);

    switch (TIMER_bitrate_mode) {
    case E_TIMER_MODE_MODULATION:
        // execute the modulation into the interrupt context
        if(TxProcess() == 1) {
           SysState = TxEnd;
        } else {
           SysState = TxWaiting;
        }
        break;
    case E_TIMER_MODE_RSSI:
    {
    #if !defined(FIPY) && !defined(LOPY4)
        sfx_u8 gpio_status;
        // Read the GPIO_STATUS register to get CCA_STATUS
        cc112xSpiReadReg(CC112X_GPIO_STATUS , &gpio_status, 1);

        /* ---------- START DEBUG  -------------------------- */
        /* Read the RSSI */
        // cc112xSpiReadReg(CC112X_RSSI1,&rssi1, 1);
        // cc112xSpiReadReg(CC112X_RSSI0,&rssi0, 1);
        /* Build the RSSI */
        // rssi = (((sfx_s8)(rssi0) & 0x78 ) >> 3) |((sfx_s8)(rssi1)<< 4);

        // rssi2 = (sfx_s8)(rssi1);

        /* Keep only the integer value */
        // rssiConverted = (rssi) * 0.0625 ;
        // rssi2Converted = (rssi2) ;
        // printf("r1=%d, r2=%d\n", rssiConverted, rssi2Converted);
        /* ---------- END DEBUG  -------------------------- */

        // Check on the GPIO3 */
        if ((gpio_status & 0x08) == 0x08) {
            // The CCA_STATUS is asserted
            TIMER_bitrate_interrupt_count++;
        } else {
            TIMER_bitrate_interrupt_count = 0;
        }
    #else
        TIMER_bitrate_interrupt_count++;
    #endif

        if (TIMER_bitrate_interrupt_count == TIMER_bitrate_nb_interrupt_to_wait_for) {
            TIMER_rssi_end = true;
            // reset the counter for the next use
            TIMER_bitrate_interrupt_count = 0;
        }
    }
        break;
    default:
        break;
    }
}

/***************************************************************************//**
*   @brief  TIMER_downlink_isr : this interrupt will be used either for
*           the 20s and 25 seconds Downlink timings
*******************************************************************************/
IRAM_ATTR void TIMER_downlink_isr (TimerHandle_t xTimer) {
    // Increment the base downlink interrupt counter
    TIMER_downlink_interrupt_count++;

    if (TIMER_downlink_interrupt_count == TIMER_downlink_nb_interrupt_to_wait_for) {
        TIMER_downlink_timeout = true;

        // Reset the counters
        TIMER_downlink_interrupt_count = 0;
        TIMER_downlink_nb_interrupt_to_wait_for = 0;
    }
}

/***************************************************************************//**
*   @brief  TIMER_carrier_sense_isr : this interrupt will be used for
 *         the carrier sense window timing
*******************************************************************************/
IRAM_ATTR void TIMER_carrier_sense_isr (TimerHandle_t xTimer) {
    // Increment the base downlink interrupt counter
    TIMER_carrier_sense_interrupt_count++;

    if (TIMER_carrier_sense_interrupt_count == TIMER_carrier_sense_nb_interrupt_to_wait_for) {
        TIMER_carrier_sense_timeout = true;

        // Reset the counters
        TIMER_carrier_sense_interrupt_count = 0;
        TIMER_carrier_sense_nb_interrupt_to_wait_for = 0;
    }
}

#if !defined(FIPY) && !defined(LOPY4)
IRAM_ATTR void TIMER_RxTx_done_isr (TimerHandle_t xTimer) {
    uint8_t status;
    cc112xSpiReadReg(CC112X_GPIO_STATUS, &status, 1);
    if (rxtx_in_progress) {
        if (!(status & 0x08)) {          // is GPIO3 de-asserted?
            // transaction done, set the packet semaphore
            rxtx_in_progress = false;
            packetSemaphore = ISR_ACTION_REQUIRED;
            xTimerStop (TIMER_RxTx_done, 0);
        }
    } else if (status & 0x08) {         // is GPIO3 asserted?
        rxtx_in_progress = true;
    }
}
#endif

/*!****************************************************************************
 * \fn void TIMER_bitrate_init(void)
 * \brief Initialize the Timer uses to produce the signal bitrate
 *        - fcc bit rate  : 600bps => 1 bit each 1.66 ms
 *        - etsi bit rate : 100bps => 1 bit each 10 ms
 ******************************************************************************/
void TIMER_bitrate_init(void) {
    TIMER_bitrate_mode = E_TIMER_MODE_MODULATION;
    if(uplink_spectrum_access == SFX_FH) {
        TIMER_bitrate_ticks = FCC_TIMER_TICKS;
    } else {
        TIMER_bitrate_ticks = ETSI_TIMER_TICKS;
    }
    xt_set_interrupt_handler(XCHAL_TIMER_INTERRUPT(TIMER_BITRATE_NUM), TIMER_bitrate_isr, NULL);
}


/*!****************************************************************************
 * \fn void TIMER_get_rssi_init(void)
 * \brief Initialize the Timer uses to produce retrieve RSSI value
 ******************************************************************************/
void TIMER_get_rssi_init(sfx_u8 time_in_milliseconds) {
    TIMER_bitrate_mode = E_TIMER_MODE_RSSI;
    TIMER_bitrate_ticks = RSSI_TIMER_TICKS;
    TIMER_bitrate_nb_interrupt_to_wait_for = ((time_in_milliseconds * 1000) / RSSI_TIMER_US) + 1;

    xt_set_interrupt_handler(XCHAL_TIMER_INTERRUPT(TIMER_BITRATE_NUM), TIMER_bitrate_isr, NULL);

    // reset the counter
    TIMER_bitrate_interrupt_count = 0;
    TIMER_rssi_end =  false;
}

void TIMER_downlinnk_timer_create (void) {
     TIMER_downlink =  xTimerCreate("DLTimer", 100 / portTICK_PERIOD_MS, pdTRUE,
                                    (void *)0, TIMER_downlink_isr);
}

void TIMER_carrier_sense_timer_create (void) {
     TIMER_clear_channel =  xTimerCreate("CSTimer", 1 / portTICK_PERIOD_MS, pdTRUE,
                                         (void *)0, TIMER_carrier_sense_isr);
}

#if !defined(FIPY) && !defined(LOPY4)
void TIMER_RxTx_done_timer_create (void) {
     TIMER_RxTx_done =  xTimerCreate("RxTxTimer", 5 / portTICK_PERIOD_MS, pdTRUE,
                                     (void *)0, TIMER_RxTx_done_isr);
}

void TIMER_RxTx_done_start (void) {
    rxtx_in_progress = false;
    xTimerStart (TIMER_RxTx_done, 0);
}

void TIMER_RxTx_done_stop (void) {
    rxtx_in_progress = false;
    xTimerStop (TIMER_RxTx_done, 0);
}
#endif

/*!****************************************************************************
 * \fn void TIMER_downlink_timing_init(u16 time_in_seconds)
 * \brief This timer will set the base interrupt for the downlink timing
 *        To have a common interrupt base for the 20s and the 25s timing,
 *        we choose to create a 1 second timer
 ******************************************************************************/
void TIMER_downlink_timing_init(sfx_u16 time_in_seconds) {
    // initialize the number of interrupt to wait for (this interrupt happens every 100ms)
    TIMER_downlink_nb_interrupt_to_wait_for = (time_in_seconds * 10) + 1;

    // reset the counter
    TIMER_downlink_interrupt_count = 0;
    TIMER_downlink_timeout = false;

    // activate the timer
    xTimerStart (TIMER_downlink, 0);
}

/*!****************************************************************************
 * \fn void TIMER_carrier_sense_init(u16 time_in_milliseconds)
 * \brief This timer will set the base interrupt for the carrier sense timing
 *        we choose to create a 1 millisecond timer
 ******************************************************************************/
void TIMER_carrier_sense_init(sfx_u16 time_in_milliseconds) {
    TIMER_carrier_sense_nb_interrupt_to_wait_for = time_in_milliseconds;
    TIMER_carrier_sense_timeout = false;
    TIMER_carrier_sense_interrupt_count = 0;

    xTimerStart (TIMER_clear_channel, 0);
}

/***************************************************************************//**
*   @brief  Start the bitrate Timer
*******************************************************************************/
void TIMER_bitrate_start (void) {
    xt_ints_on(1 << XCHAL_TIMER_INTERRUPT(TIMER_BITRATE_NUM));
    xthal_set_ccompare(TIMER_BITRATE_NUM, xthal_get_ccount() + TIMER_bitrate_ticks);
}


/***************************************************************************//**
*   @brief Stop the bitrate timer
*******************************************************************************/
void TIMER_bitrate_stop(void) {
    xt_ints_off(1 << XCHAL_TIMER_INTERRUPT(TIMER_BITRATE_NUM));
}

/*!****************************************************************************
 * \fn void TIMER_downlink_timing_stop( void )
 * \brief This timer will stop the base interrupt for the downlink timing
 ******************************************************************************/
void TIMER_downlink_timing_stop(void) {
    // Stop the timer
    xTimerStop (TIMER_downlink, 0);
    TIMER_downlink_timeout = FALSE;
}

/*!****************************************************************************
 * \fn void TIMER_carrier_sense_stop( void )
 * \brief This timer will stop the carrier sense timer
 ******************************************************************************/
void TIMER_carrier_sense_stop(void) {
    xTimerStop (TIMER_clear_channel, 0);
    TIMER_carrier_sense_timeout = FALSE;
}

/*!****************************************************************************
 * \fn void TIMER_get_rssi_start(void)
 * \brief  Start the RSSI Timer
 ******************************************************************************/
void TIMER_get_rssi_start(void) {
    xt_ints_on(1 << XCHAL_TIMER_INTERRUPT(TIMER_BITRATE_NUM));
    xthal_set_ccompare(TIMER_BITRATE_NUM, xthal_get_ccount() + TIMER_bitrate_ticks);
}

/*!****************************************************************************
 * \fn void TIMER_get_rssi_stop(void)
 * \brief Stop the RSSI timer
 ******************************************************************************/
void TIMER_get_rssi_stop(void) {
    xt_ints_off(1 << XCHAL_TIMER_INTERRUPT(TIMER_BITRATE_NUM));
}

/**************************************************************************//**
* Close the Doxygen group.
* @}
******************************************************************************/
