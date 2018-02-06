/*!****************************************************************************
 * file radio.h
 * @brief Manage the low level modulation and configuration of the TI chipset
 * @author SigFox Test and Validation team
 * @version 0.1
 ******************************************************************************
 * \section License
 * <b>(C) Copyright 2015 SIGFOX, http://www.sigfox.com</b>
 ******************************************************************************
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * DISCLAIMER OF WARRANTY/LIMITATION OF REMEDIES: SIGFOX has no
 * obligation to support this Software. SIGFOX is providing the
 * Software "AS IS", with no express or implied warranties of any kind,
 * including, but not limited to, any implied warranties of merchantability
 * or fitness for any particular purpose or warranties against infringement
 * of any proprietary rights of a third party.
 *
 * SIGFOX will not be liable for any consequential, incidental, or
 * special damages, or any other relief, or for any claim by any third party,
 * arising from your use of this Software.
 *
 */

#ifndef RADIO_H
#define RADIO_H


#include "sigfox_api.h"
#if defined (FIPY)
#include "sx1272/sx1272Regs-Fsk.h"
#include "sx1272/sx1272Regs-LoRa.h"
#elif defined (LOPY4)
#include "sx1276/sx1276Regs-Fsk.h"
#include "sx1276/sx1276Regs-LoRa.h"
#endif
#include "targets/hal_spi_rf_trxeb.h"



/******************************************************************************
* DEFINES
*/
#define SX1272X_WRITE_ACCESS                0x80

/******************************************************************************
* MACROS
*/
// Fast write on SPI for modulation
#define SX127X8BitWrite(register, data) \
{ \
  GPIO_REG_WRITE(GPIO_OUT_W1TC_REG, 1 << 18); \
  SpiOut(SpiNum_SPI3, (data << 8) | SX1272X_WRITE_ACCESS | register); \
  GPIO_REG_WRITE(GPIO_OUT_W1TS_REG, 1 << 18); \
}

void RADIO_init_chip(sfx_rf_mode_t rf_mode);
void RADIO_close_chip(void);
void RADIO_change_frequency(unsigned long ul_Freq);
void RADIO_start_rf_carrier(void);
void RADIO_stop_rf_carrier(void);
void RADIO_stop_unmodulated_cw(unsigned long ul_Freq);
void RADIO_start_unmodulated_cw(unsigned long ul_Freq);
void RADIO_modulate(void);
void RADIO_warm_up_crystal (unsigned long ul_Freq);
void RADIO_reset_registers (void);

extern uint8_t  packetSemaphore;

#define ISR_ACTION_REQUIRED          1
#define ISR_IDLE                     0

#define MAX_PA_VALUE_ETSI            99
#define MAX_PA_VALUE_FCC             145
#define STEP_HIGH_ETSI               53
#define STEP_HIGH_FCC                56
#define MIN_PA_VALUE                 0


static const registerSetting_t HighPerfModeRx[] =
{
	/* Register address     ,   Value    */
	{ REG_BITRATEMSB ,       0xD0 }, /* RegBitrateMsb */
	{ REG_BITRATELSB ,       0x55 }, /* RegBitrateLsb
									  * Bitrate = Fxosc / ( bitrate_reg_value + bitratefrac / 16 )
									  * with value : Fxosc = 32 MHz and bitratefrac = 0
									  * => The Bitrate is then set to 600 bps */ 
	{ REG_FDEVMSB    ,       0x00 }, /* RegFdevMsb */ 
	{ REG_FDEVLSB    ,       0x0D }, /* RegFdevLsb
									  * LSB of the frequency deviation
									  * Fdev = Fstep * Fdev_reg_value
									  * with Fstep = 61 ( Fxosc / 2**19 )
									  * => the Frequency Deviation is set to ~ 800Hz
									  */

	{ REG_PACONFIG,          0x8F }, /* RegPaConfig
									  * PaSelect = 1b : PA_BOOST
									  * Unused = 000b
									  * OutputPower = 0xF : Pout = 2 + OutputPower [dBm], on PA_BOOST pin */

	{ REG_OCP,               0x37 }, /* RegOcp
									  * unused : 00b
									  * OcpOn = 1b : Enables overload current protection (OCP) for the PA
									  * OcpTrim = 0111b : Trimming of OCP current
									  I max = 45+5*OcpTrim [mA] */

	{ REG_LNA,               0x23 }, /* RegLna
									  * LnaGain = 001b : G1 = highest gain
									  * Unused = 000b
									  * LnaBoost = 11b : Improved sensitivity */ 

	{ REG_RXCONFIG,          0x1E }, /* RegRxConfig
									  * RestartRxOnCollision = 0b : No automatic Restart
									  * RestartRxWithoutPllLock = 0b : No Manual restart of the receiver
									  * RestartRxWithPllLock = 0b : No Manual restart of the receiver
									  * AfcAutoOn = 1b : AFC is performed at each receiver startup
									  * AgcAutoOn = 1b : LNA gain is controlled by the AGC
									  * RxTrigger = 110b : Selects the event triggering AGC and/or AFC at receiver startup. => PreambleDetect */ 

	{ REG_RXBW,              0x17 }, /* RegRxBw
									  * unused = 0b
									  * reserved = 00b
									  * RxBwMant = 10b : Channel filter bandwidth control: RxBwMant = 24
									  * RxBwExp = 111b : Channel filter bandwidth control:
									  RxBw = Fxosc / ( RxBwMant * 2**(RxBwExp + 2)
									  => RxBw = 32000000 / (24 *2**9)  = 2604 Hz */

	{ REG_AFCBW,             0x17 }, /* RegAfcBw
									  * reserved = 000b
									  * RxBwMantAfc = 10b : RxBwMant parameter used during the AFC : RxBwMan = 24
									  * RxBwExpAfc = 111b : RxBwExp parameter used during the AFC RxBwExp = 7 */
	{ REG_OOKAVG,            0x11 }, /* RegOokAvg
									  * OokPeakThreshDec = 000b : Period of decrement of the RSSI threshold in the OOK demodulator : Once per chip
									  * reserved = 1b
									  * OokAverageOffset = 00b : Static offset added to the threshold in average mode in order to
									  reduce glitching activity (OOK only) : 0 dbB
									  * OokAverageThreshFilt = 01b Filter coefficients in average mode of the OOK demodulator : f C ≈ chip rate / 8.π */

	{ REG_AFCMSB,            0x00 }, /* RegAfcMsb - AfcValue(15:8) */
	{ REG_AFCLSB,            0x00 }, /* RegAfcLsb - AfcValue(7:0) : AfcValue, 2’s complement format. Can be used to overwrite the current AFC value */

	{ REG_FEIMSB,            0x00 }, /* RegFeiMsb */ // TBD CHECKED - was not set on greg config 
	{ REG_FEILSB,            0x25 }, /* RegFeiLsb
									  * measured frequency offset, 2’s complement
									  * => Frequency error = FeiValue x Fstep
									  * => Frequency error = 0x25 * 61 = 2257 Hz */

	{ REG_PREAMBLEDETECT,    0xAA }, /* RegPreambleDetect
									  * PreambleDetectorOn = 1b : Enables Preamble detector when set to 1
									  * PreambleDetectorSize = 01b : 2bytes (Number of Preamble bytes to detect to trigger an interrupt)
									  * PreambleDetectorTol = 01010b : Number or chip errors tolerated over PreambleDetectorSize. ( default value is 0x0A ) */

	{ REG_PREAMBLELSB,       0x02 }, /* RegPreambleLsb - TBD - not needed as this is preamble to be sent ... and we do not use GFSK in transmit mode 
									  * PreambleSize(7:0)
									  * Size of the preamble to be sent (from TxStartCondition fulfilled). */ 

	{ REG_SYNCCONFIG,        0x91 }, /* RegSyncConfig
									  * AutoRestartRxMode = 10b : Controls the automatic restart of the receiver after the reception of a valid packet (PayloadReady or CrcOk): automatic restart On, wait for the PLL to lock (frequency changed)
									  * PreamblePolarity = 0b : Sets the polarity of the Preamble : 0xAA
									  * SyncOn = 1b : Enables the Sync word generation and detection
									  * FifoFillCondition = 0b : FIFO filling condition: if SyncAddress interrupt occurs
									  * SyncSize = 001b : Size of the Sync word: ( SyncSize + 1) bytes, ( SyncSize ) bytes if ioHomeOn =1
									  * => 2 bytes of Synchro Frame : 0xB227 */

	{ REG_SYNCVALUE1,        0xB2 }, /* RegSyncValue1 : 1st byte of Sync word. (MSB byte) */
	{ REG_SYNCVALUE2,        0x27 }, /* RegSyncValue2 : 2nd byte of Sync word */

	{ REG_PACKETCONFIG1,     0x08 }, /* RegPacketConfig1
									  * PacketFormat = 0b : Defines the packet format used: Fixed length
									  * DcFree = 00b : Defines DC-free encoding/decoding performed: None (Off)
									  * CrcOn = 0b : CRC calculation/check (Tx/Rx) Off
									  * CrcAutoClearOff = 1b : Defines the behavior of the packet handler when CRC check fails: 1 = Do not clear FIFO. PayloadReady interrupt issued.
									  * AddressFiltering = 00b : Defines address based filtering in Rx : None (Off)
									  * CrcWhiteningType = 0b : Selects the CRC and whitening algorithms: CCITT CRC implementation with standard whitening */

	{ REG_PACKETCONFIG2,     0x40 }, /* RegPacketConfig2
									  * unused = 0b 
									  * DataMode = 1b : Data processing mode: Packet mode
									  * IoHomeOn = 0b : Disable the io-homecontrol ® compatibility mode 
									  * IoHomePowerFrame = 0b reserved - Linked to io-homecontrol ® compatibility mode
									  * BeaconOn = 0b : Disable the Beacon mode in Fixed packet format
									  * PayloadLength(10:8) : 000b : Packet Length Most significant bits */

	{ REG_PAYLOADLENGTH,     0x0F }, /* PayloadLength(7:0) : If PacketFormat = 0 (fixed), payload length. : 15 bytes of payload */

	{ REG_NODEADRS,          0x00 }, /* RegNodeAdrs TBD Karine : probably not needed
									  * NodeAddress =0x00 : Node address used in address filtering. */

	{ REG_BROADCASTADRS,     0x00 }, /* RegBroadcastAdrs
									  * BroadcastAddress = 0x00 : Broadcast address used in address filtering. */

	{ REG_FIFOTHRESH,        0x8F }, /* RegFifoThresh
									  * TxStartCondition = 1b : Defines the condition to start packet transmission: 1 FifoEmpty goes low (i.e. at least one byte in the FIFO)
									  * unused  = 0b 
									  * FifoThreshold = 0xF : Used to trigger FifoLevel interrupt, when: number of bytes in FIFO >= FifoThreshold + 1 : TBD Karine checked if it should not be 0xE ... */

	{ REG_SEQCONFIG1,         0x00 }, /* RegSeqConfig1 - TBD Karine not sure this is needed */
	{ REG_SEQCONFIG2,         0x00 }, /* RegSeqConfig2 - TBD Karine not sure this is needed */
	{ REG_TIMERRESOL,         0x00 }, /* RegTimerResol - TBD Karine not sure this is needed */
	{ REG_TIMER1COEF,         0xF5 }, /* RegTimer1Coef - TBD Karine not sure this is needed */
	{ REG_TIMER2COEF,         0x20 }, /* RegTimer2Coef - TBD Karine not sure this is needed */
	{ REG_IMAGECAL,           0x02 }, 
	{ REG_TEMP, 			  0xF2 },
	{ REG_LOWBAT, 			  0x02 },

	{ REG_IRQFLAGS1,          0x18 }, /* RegIrqFlags1
									   * bit7 - read bit : ModeReady  
									   * bit6 - read bit : RxReady
									   * bit5 - read bit : TxReady
									   * bit4 - read bit : PllLock
									   * bit3 : Rssi : Set in Rx when the RssiValue exceeds RssiThreshold.
									   Cleared when leaving Rx or setting this bit to 1.
									   * bit2 - read bit : Timeout
									   * bit1 : PreambleDetect : Set when the Preamble Detector has found valid Preamble.  bit clear when set to 1
									   * bit0 : SyncAddressMatch */

	{ REG_IRQFLAGS2,          0x40 }, /* RegIrqFlags2
									   * bit7 - read bit : FifoFull : Set when FIFO is full (i.e. contains 66 bytes), else cleared.
									   * bit6 - read bit : FifoEmpty : Set when FIFO is empty, and cleared when there is at least 1 byte in the FIFO.
									   * bit5 - FifoLevel : Set when the number of bytes in the FIFO strictly exceeds FifoThreshold , else cleared 	
									   * bit4 : FifoOverrun : Set when FIFO overrun occurs. (except in Sleep mode) Flag(s) and FIFO are cleared when this bit is set.
									   The FIFO then becomes immediately available for the next transmission / reception.
									   * bit3 - read bit : PacketSent : Set in Tx when the complete packet has been sent.
									   * bit2 - read bit : PayloadReady : Set in Rx when the payload is ready
									   * bit1 - read bit : CrcOk
									   * bit0 : LowBat */	

	{ REG_DIOMAPPING1, 			0x00 },
	{ REG_DIOMAPPING2, 			0x00 },
	{ REG_VERSION, 				0x21 },
	{ REG_AGCREF, 				0x1C },
	{ REG_AGCTHRESH1, 			0x0E },
	{ REG_AGCTHRESH2, 			0x5B },
	{ REG_AGCTHRESH3, 			0xDB },
	{ REG_TCXO,             	0x09 }, /* RegTcxo */
	{ REG_BITRATEFRAC,      	0x00 }, /* BitRateFrac used with bitrate - TBD Karine why is it initially set to 9 ?? => Set it to 0 to have a 600 bps */
};

#endif
