#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "esp32_mphal.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_attr.h"

#include "gpio.h"
#include "spi.h"
#include "esp_attr.h"
#include "machpin.h"
#include "pins.h"

#include "soc/gpio_sig_map.h"
#include "soc/dport_reg.h"

#include "ksz8851.h"
#include "ksz8851conf.h"

#define SPI_BEGIN     0
#define SPI_CONTINUE  1
#define SPI_END       2
#define SPI_COMPLETE  3

//#define MSG(fmt, ...) printf("[%u] ksz8851: " fmt, mp_hal_ticks_ms(), ##__VA_ARGS__)
#define MSG(fmt, ...) (void)0

static uint16_t length_sum;
static uint8_t frameID = 0;
static DRAM_ATTR ksz8851_evt_cb_t evt_cb_func = NULL;

static IRAM_ATTR void gpio_set_value(pin_obj_t *pin_o, uint32_t value);
static IRAM_ATTR void ksz8851ProcessInterrupt(void);

static void init_spi(void) {
    portDISABLE_INTERRUPTS();
    // this is SpiNum_SPI2
    DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_SPI_CLK_EN_2);
    DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_SPI_RST_2);

    // configure the SPI port
    spi_attr_t spi_attr = {.mode = SpiMode_Master, .subMode = SpiSubMode_0, .speed = SpiSpeed_8MHz,
                           .bitOrder = SpiBitOrder_MSBFirst, .halfMode = SpiWorkMode_Full};

    spi_init(KSZ8851_SPI_NUM, &spi_attr);
    while (READ_PERI_REG(SPI_CMD_REG(KSZ8851_SPI_NUM)) & SPI_USR);  // wait for SPI not busy

    // set a NULL command
    CLEAR_PERI_REG_MASK(SPI_USER_REG(KSZ8851_SPI_NUM), SPI_USR_COMMAND);
    SET_PERI_REG_BITS(SPI_USER2_REG(KSZ8851_SPI_NUM), SPI_USR_COMMAND_BITLEN, 0, SPI_USR_COMMAND_BITLEN_S);

    // set a NULL address
    CLEAR_PERI_REG_MASK(SPI_USER_REG(KSZ8851_SPI_NUM), SPI_USR_ADDR);
    SET_PERI_REG_BITS(SPI_USER1_REG(KSZ8851_SPI_NUM), SPI_USR_ADDR_BITLEN, 0, SPI_USR_ADDR_BITLEN_S);

    // enable MOSI
    SET_PERI_REG_MASK(SPI_USER_REG(KSZ8851_SPI_NUM), SPI_USR_MOSI);

    // set the data send buffer length. The max data length 64 bytes.
    SET_PERI_REG_BITS(SPI_MOSI_DLEN_REG(KSZ8851_SPI_NUM), SPI_USR_MOSI_DBITLEN, 7, SPI_USR_MOSI_DBITLEN_S);
    SET_PERI_REG_BITS(SPI_MISO_DLEN_REG(KSZ8851_SPI_NUM), SPI_USR_MISO_DBITLEN, 7, SPI_USR_MISO_DBITLEN_S);

    // assign the SPI pins to the GPIO matrix and configure the AF
    pin_config(KSZ8851_MISO_PIN, VSPIQ_IN_IDX, -1, GPIO_MODE_INPUT, MACHPIN_PULL_NONE, 0);
    pin_config(KSZ8851_MOSI_PIN, -1, VSPID_OUT_IDX, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 0);
    pin_config(KSZ8851_SCLK_PIN, -1, VSPICLK_OUT_IDX, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 0);
    pin_config(KSZ8851_NSS_PIN, -1, -1, GPIO_MODE_OUTPUT, MACHPIN_PULL_UP, 1);
    pin_config(KSZ8851_RST_PIN, -1, -1, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 0);
    pin_config((&PIN_MODULE_P20), -1, -1, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 0);

    gpio_set_value( (&PIN_MODULE_P20), 1 );
    portENABLE_INTERRUPTS();
}

/* spi_byte() sends one byte (outdat) and returns the received byte */
static IRAM_ATTR uint8_t spi_byte(uint8_t outdat) {
    // load the send buffer
    WRITE_PERI_REG(SPI_W0_REG(KSZ8851_SPI_NUM), outdat);
    // start to send data
    SET_PERI_REG_MASK(SPI_CMD_REG(KSZ8851_SPI_NUM), SPI_USR);
    while (READ_PERI_REG(SPI_CMD_REG(KSZ8851_SPI_NUM)) & SPI_USR);
    // read data out
    return READ_PERI_REG(SPI_W0_REG(KSZ8851_SPI_NUM));
}

static IRAM_ATTR void gpio_set_value(pin_obj_t *pin_o, uint32_t value) {
    // set the pin value
    if (value) {
        pin_o->value = 1;
    } else {
        pin_o->value = 0;
    }
    pin_set_value(pin_o);
}

/* spi_op() performs register reads, register writes, FIFO reads, and
 * FIFO writes.  It can also either:
 * Do one complete SPI transfer (with CSN bracketing all of the SPI bytes),
 * Start an SPI transfer (asserting CSN but not negating it),
 * Continue an SPI transfer (leaving CSN in the asserted state), or
 * End an SPI transfer (negating CSN at the end of the transfer).
 */
static IRAM_ATTR void spi_op(uint8_t phase, uint16_t cmd, uint8_t *buf, uint16_t len) {
    uint16_t opcode;
    uint16_t ii;

    opcode = cmd & OPCODE_MASK;

    if ((phase == SPI_BEGIN) || (phase == SPI_COMPLETE)) {
        /* Drop CSN */
        gpio_set_value(KSZ8851_NSS_PIN, 0);

        /* Command phase */
        spi_byte(cmd >> 8);
        if ((opcode == IO_RD) || (opcode == IO_WR)) {
            /* Do extra byte for command phase */
            spi_byte(cmd & 0xff);
        }
    }

    /* Data phase */
    if ((opcode == IO_RD) || (opcode == FIFO_RD)) {
        for (ii = 0; ii < len; ii++)
            *buf++ = spi_byte(0);
    } else {
        for (ii = 0; ii < len; ii++)
            spi_byte(*buf++);
    }

    if ((phase == SPI_END) || (phase == SPI_COMPLETE)) {
        /* Raise CSN */
        gpio_set_value(KSZ8851_NSS_PIN, 1);
    }
}


/* interrupt handler
all we do is add a cmd into the queue to process the interrupt

We intentionally do not read over the spi bus, because that can conflict with ongoing tx/rx buffer access
*/
static IRAM_ATTR void ksz8851ProcessInterrupt(void) {
    evt_cb_func(KSZ8851_HW_INT);
}

/* ksz8851_regrd() will read one 16-bit word from reg. */
uint16_t IRAM_ATTR ksz8851_regrd(uint16_t reg) {
    uint16_t cmd;
    uint8_t inbuf[2];
    uint16_t rddata;

    /* Move register address to cmd bits 9-2, make 32-bit address */
    cmd = (reg << 2) & 0x3f0;

    /* Add byte enables to cmd */
    if (reg & 2) {
        /* Odd word address writes bytes 2 and 3 */
        cmd |= (0xc << 10);
    } else {
        /* Even word address write bytes 0 and 1 */
        cmd |= (0x3 << 10);
    }

    /* Add opcode to cmd */
    cmd |= IO_RD;

    spi_op(SPI_COMPLETE, cmd, inbuf, 2);

    /* Byte 0 is first in, byte 1 is next */
    rddata = (inbuf[1] << 8) | inbuf[0];

    return rddata;
}

/* ksz8851_regwr() will write one 16-bit word (wrdata) to reg. */
void IRAM_ATTR ksz8851_regwr(uint16_t reg, uint16_t wrdata) {
    uint16_t cmd;
    uint8_t outbuf[2];

    /* Move register address to cmd bits 9-2, make 32-bit address */
    cmd = (reg << 2) & 0x3f0;

    /* Add byte enables to cmd */
    if (reg & 2) {
        /* Odd word address writes bytes 2 and 3 */
        cmd |= (0xc << 10);
    } else {
        /* Even word address write bytes 0 and 1 */
        cmd |= (0x3 << 10);
    }

    /* Add opcode to cmd */
    cmd |= IO_WR;

    /* Byte 0 is first out, byte 1 is next */
    outbuf[0] = wrdata & 0xff;
    outbuf[1] = wrdata >> 8;

    spi_op(SPI_COMPLETE, cmd, outbuf, 2);
}

/* spi_setbits() will set all of the bits in bits_to_set in register
 * reg. */
void spi_setbits(uint16_t reg, uint16_t bits_to_set) {
    uint16_t temp;

    temp = ksz8851_regrd(reg);
    temp |= bits_to_set;
    ksz8851_regwr(reg, temp);
}

/* spi_clrbits() will clear all of the bits in bits_to_clr in register
 * reg. */
void spi_clrbits(uint16_t reg, uint16_t bits_to_clr) {
    uint16_t temp;

    temp = ksz8851_regrd(reg);
    temp &= ~bits_to_clr;
    ksz8851_regwr(reg, temp);
}

/* ksz8851GetLinkStatus() get link status.
 */
bool ksz8851GetLinkStatus(void) {
    uint16_t port_status;
    port_status = ksz8851_regrd(REG_PORT_STATUS);
    return (port_status & PORT_STATUS_LINK_GOOD);
}

/* ksz8851PowerSavingMode() go to power save mode.
 */
void ksz8851PowerDownMode(void) {
    portDISABLE_INTERRUPTS();
    spi_setbits(REG_PHY_CNTL, PHY_POWER_DOWN);
    portENABLE_INTERRUPTS();
}

/* ksz8851SoiInit() initializes the spi for ksz8851.
 */
void ksz8851SpiInit(void) {
    init_spi();
}

/* ksz8851Init() initializes the ksz8851.
 */
void ksz8851Init(void) {
    uint16_t dev_id;

    /* Make sure we get a valid chip ID before going on */
    do {
        gpio_set_value(KSZ8851_RST_PIN, 0);
        ets_delay_us(20);
        gpio_set_value(KSZ8851_RST_PIN, 1);
        ets_delay_us(1000 * 100);
        /* Read device chip ID */
        portDISABLE_INTERRUPTS();
        dev_id = ksz8851_regrd(REG_CHIP_ID);
        portENABLE_INTERRUPTS();

        if ((dev_id & CHIP_ID_MASK) != CHIP_ID_8851_16) {
            printf("Expected Device ID 0x%x, got 0x%x\n", CHIP_ID_8851_16, dev_id);
        }
    } while ((dev_id & CHIP_ID_MASK) != CHIP_ID_8851_16);

    portDISABLE_INTERRUPTS();
    uint16_t mem_self_test = ksz8851_regrd(REG_MEM_BIST_INFO);
    uint16_t mem_self_test_done = TX_MEM_TEST_FINISHED | RX_MEM_TEST_FINISHED;
    while ( (mem_self_test & mem_self_test_done) != mem_self_test_done ) {
        vTaskDelay(1 / portTICK_PERIOD_MS);
        mem_self_test = ksz8851_regrd(REG_MEM_BIST_INFO);
    }
    portENABLE_INTERRUPTS();
    MSG("Init: 0x%x\n", mem_self_test);

#define DISABLE_100MBIT
#ifdef DISABLE_100MBIT
    portDISABLE_INTERRUPTS();
    uint16_t auto_negotiation = ksz8851_regrd(REG_PHY_AUTO_NEGOTIATION);
    auto_negotiation &= ~PHY_AUTO_NEG_100BTX_FD;
    auto_negotiation &= ~PHY_AUTO_NEG_100BTX;
    ksz8851_regwr(REG_PHY_AUTO_NEGOTIATION, auto_negotiation);
    portENABLE_INTERRUPTS();
    MSG("Init: AN=0x%x\n", auto_negotiation);
#endif

    portDISABLE_INTERRUPTS();

    /* Write QMU MAC address (low) */
    ksz8851_regwr(REG_MAC_ADDR_01, (ethernet_mac[4] << 8) | ethernet_mac[5]);
    /* Write QMU MAC address (middle) */
    ksz8851_regwr(REG_MAC_ADDR_23, (ethernet_mac[2] << 8) | ethernet_mac[3]);
    /* Write QMU MAC address (high) */
    ksz8851_regwr(REG_MAC_ADDR_45, (ethernet_mac[0] << 8) | ethernet_mac[1]);

    /* Enable QMU Transmit Frame Data Pointer Auto Increment */
    ksz8851_regwr(REG_TX_ADDR_PTR, ADDR_PTR_AUTO_INC);

    /* Enable QMU TxQ Auto-Enqueue frame */
    //ksz8851_regwr(REG_TXQ_CMD, TXQ_AUTO_ENQUEUE);

    /* Enable QMU Transmit:
     * flow control,
     * padding,
     * CRC, and
     * IP/TCP/UDP/ICMP checksum generation.
     */
    ksz8851_regwr(REG_TX_CTRL, DEFAULT_TX_CTRL);

    /* Enable QMU Receive Frame Data Pointer Auto Increment */
    ksz8851_regwr(REG_RX_ADDR_PTR, ADDR_PTR_AUTO_INC);

    /* Configure Receive Frame Threshold for one frame */
    ksz8851_regwr(REG_RX_FRAME_CNT_THRES, 1);

    /* Enable QMU Receive:
     * flow control,
     * receive all broadcast frames,
     * receive unicast frames, and
     * IP/TCP/UDP/ICMP checksum generation.
     */
    ksz8851_regwr(REG_RX_CTRL1, DEFAULT_RX_CTRL1);

    /* Enable QMU Receive:
     * ICMP/UDP Lite frame checksum verification,
     * UDP Lite frame checksum generation, and
     * IPv6 UDP fragment frame pass.
     */
    ksz8851_regwr(REG_RX_CTRL2, DEFAULT_RX_CTRL2 | RX_CTRL_BURST_LEN_FRAME);

    /* Enable QMU Receive:
     * IP Header Two-Byte Offset,
     * Receive Frame Count Threshold, and
     * RXQ Auto-Dequeue frame.
     */
    ksz8851_regwr(REG_RXQ_CMD, RXQ_CMD_CNTL);

    /*spi_setbits(REG_PORT_CTRL, PORT_AUTO_MDIX_DISABLE);
    spi_setbits(REG_PHY_CNTL, PHY_AUTO_MDIX_DISABLE);

    ets_delay_us(1000);

    spi_setbits(REG_PORT_LINK_MD , PORT_START_CABLE_DIAG);

    do
    {
        ets_delay_us(1000);

    }while((ksz8851_regrd(REG_PORT_LINK_MD) & PORT_START_CABLE_DIAG));

    uint16_t diag = ksz8851_regrd(REG_PORT_LINK_MD);

    printf("=======CABLE DIAG========\n");
    printf("Cable OC/SC: %X\n", diag & PORT_CABLE_DIAG_RESULT);

    spi_clrbits(REG_PHY_CNTL, PHY_AUTO_MDIX_DISABLE);

    spi_clrbits(REG_PORT_CTRL, PORT_AUTO_MDIX_DISABLE);*/

    /* restart Port 1 auto-negotiation */
    spi_setbits(REG_PORT_CTRL, PORT_AUTO_NEG_RESTART);

    /* Clear the interrupts status */
    ksz8851_regwr(REG_INT_STATUS, 0xffff);
    /* Enable ext interrupt */
    pin_irq_disable(KSZ8851_INT_PIN);
    pin_extint_register(KSZ8851_INT_PIN, GPIO_INTR_NEGEDGE, 0);
    machpin_register_irq_c_handler(KSZ8851_INT_PIN, (void *)ksz8851ProcessInterrupt);
    pin_irq_enable(KSZ8851_INT_PIN);

//#define POWERSAVE
#ifdef POWERSAVE
    //set power save mode when cable is disconnected or link down
    uint16_t pwrctrl;
    pwrctrl = ksz8851_regrd(REG_POWER_CNTL);
    pwrctrl &= ~0x0003;
    pwrctrl |= POWER_STATE_D1;
    ksz8851_regwr(REG_POWER_CNTL, pwrctrl);
    spi_setbits(REG_PORT_LINK_MD, PORT_POWER_SAVE_MODE);
#endif

    /* Enable Link change interrupt , enable  tx/Rx interrupts */
    spi_setbits(REG_INT_MASK, INT_MASK);

    /* Enable QMU Transmit */
    spi_setbits(REG_TX_CTRL, TX_CTRL_ENABLE);

    /* Enable QMU Receive */
    spi_setbits(REG_RX_CTRL1, RX_CTRL_ENABLE);

    portENABLE_INTERRUPTS();
}

/* ksz8851BeginPacketSend() starts the packet sending process.  First,
 * it checks to see if there's enough space in the ksz8851 to send the
 * packet.  If not, it waits until there is enough room.
 * Once there is enough room, it enables TXQ write access and sends
 * the 4-byte control word to the ksz8851.
 */
void ksz8851BeginPacketSend(unsigned int packetLength)
{
    uint16_t txmir;
    uint16_t isr;
    uint8_t outbuf[4];

    /* Check if TXQ memory size is available for this transmit packet */
    txmir = ksz8851_regrd(REG_TX_MEM_INFO) & TX_MEM_AVAILABLE_MASK;
    //MSG("Send Begin(%u): %u %u\n", packetLength, txmir, (txmir < packetLength+4));
    if (txmir < packetLength + 4) {
        /* Not enough space to send packet. */

        /* Enable TXQ memory available monitor */
        ksz8851_regwr(REG_TX_TOTAL_FRAME_SIZE, packetLength + 4);

        spi_setbits(REG_TXQ_CMD, TXQ_MEM_AVAILABLE_INT);

        /* When the isr register has the TXSAIS bit set, there's
        * enough room for the packet.
        */
        do {
            isr = ksz8851_regrd(REG_INT_STATUS);
        } while (!(isr & INT_TX_SPACE));

        /* Disable TXQ memory available monitor */
        spi_clrbits(REG_TXQ_CMD, TXQ_MEM_AVAILABLE_INT);

        /* Clear the flag */
        isr |= INT_TX_SPACE;
        ksz8851_regwr(REG_INT_STATUS, isr);
    }

    /* Enable TXQ write access */
    spi_setbits(REG_RXQ_CMD, RXQ_START);

    /* Write control word and byte count */
    outbuf[0] = frameID++ & 0x3f;
    outbuf[1] = 0;
    outbuf[2] = packetLength & 0xff;
    outbuf[3] = packetLength >> 8;

    spi_op(SPI_BEGIN, FIFO_WR, outbuf, 4);

    length_sum = 0;
}

/* ksz8851SendPacketData() is used to send the payload of the packet.
 * It may be called one or more times to completely transfer the
 * packet.
*/
void ksz8851SendPacketData(unsigned char *localBuffer, unsigned int length) {

    length_sum += length;
    //MSG("Send Data(buf, %u):%u\n", length, length_sum);

    spi_op(SPI_CONTINUE, FIFO_WR, localBuffer, length);
}

/* ksz8851EndPacketSend() is called to complete the sending of the
 * packet.  It pads the payload to round it up to the nearest DWORD,
 * then it diables the TXQ write access and isues the transmit command
 * to the TXQ.  Finally, it waits for the transmit command to complete
 * before exiting.
*/
void ksz8851EndPacketSend(void) {
    uint32_t dummy = 0;

    //MSG("Send End:length_sum = %d\n", length_sum);

    /* Calculate how many bytes to get to DWORD */
    length_sum = -length_sum & 3;

    //printf("ksz8851EndPacketSend():extra bytes = %d\n", length_sum);

    /* Finish SPI FIFO_WR transaction */
    spi_op(SPI_END, FIFO_WR, (uint8_t *)&dummy, length_sum);

    /* Disable TXQ write access */
    spi_clrbits(REG_RXQ_CMD, RXQ_START);

    /* Issue transmit command to the TXQ */
    spi_setbits(REG_TXQ_CMD, TXQ_ENQUEUE);

    /* Wait until transmit command clears */
    while (ksz8851_regrd(REG_TXQ_CMD) & TXQ_ENQUEUE);
}

/* ksz8851RetrievePacketData() is used to retrieve the payload of a
 * packet.  It may be called as many times as necessary to retrieve
 * the entire payload.
 */
void ksz8851RetrievePacketData(unsigned char *localBuffer, unsigned int *length, uint16_t frameCnt, uint16_t frameCntTotal) {
    //spi_op(SPI_CONTINUE, FIFO_RD, localBuffer, length);
    size_t rxPacketLength = 0;
    uint16_t status;
    uint8_t dummy[4];

    *length = 0;
    //Read received frame status from RXFHSR
    status = ksz8851_regrd(REG_RX_FHR_STATUS);
    //MSG("Retr Data s=0x%x\n", status);

    //Read received frame byte size from RXFHBCR
    // this includes:
    // 4-byte CRC and
    // 2-byte IP Header Two-Byte Offset Enable (RXQ_TWOBYTE_OFFSET)
    rxPacketLength = ksz8851_regrd(REG_RX_FHR_BYTE_CNT) & RX_BYTE_CNT_MASK;
    // align with 4-byte (DWord) boundary
    size_t lengthInDWord = ( (rxPacketLength +3) >> 2);
    size_t lengthInByte = ( lengthInDWord * 4 );

    //Make sure the frame is valid
    if (status & RX_VALID)
    {
        //Check error flags
        if (!(status & (RX_ERRORS)))
        {

            //Ensure the frame size is acceptable
            if(rxPacketLength > 4 && rxPacketLength <= ETHERNET_RX_PACKET_BUFF_SIZE)
            {
                //Reset QMU RXQ frame pointer to zero
                //spi_clrbits(REG_RX_ADDR_PTR, ADDR_PTR_MASK);
                ksz8851_regwr(REG_RX_ADDR_PTR, 0x4000);
                //Enable RXQ read access
                spi_setbits(REG_RXQ_CMD, RXQ_START);
                /* Read 4-byte garbage - must read out dummy 4-byte */
                spi_op(SPI_BEGIN, FIFO_RD, dummy, 4);
                /* Read out 2-byte Status Word and
                   Read out 2-byte Byte Count */
                spi_op(SPI_CONTINUE, FIFO_RD, dummy, 4);

                /* Read 2-byte IP header offset bytes */
                spi_op(SPI_CONTINUE, FIFO_RD, dummy, 2);
                lengthInByte -= 2;
                rxPacketLength -= 2;

                //Read data
                spi_op(SPI_END, FIFO_RD, localBuffer, lengthInByte);
                //End RXQ read access
                spi_clrbits(REG_RXQ_CMD, RXQ_START);
                /* pass correct frame to upper layer */
                rxPacketLength -= 4; // subtract 4-bytes for CRC
                *length = rxPacketLength;
                return;
            }
            else
            {
                // no printf inside portDISABLE_INTERRUPTS()
                //MSG("Retr[%u/%u] unacceptable frame size: n=%u (4,%u) s=0x%02x a=0x%02x\n", frameCnt, frameCntTotal, n, ETHERNET_RX_PACKET_BUFF_SIZE, status, ksz8851_regrd(REG_RX_ADDR_PTR));
            }
        }
        else
        {
            // no printf inside portDISABLE_INTERRUPTS()
            //MSG("Retr[%u/%u] errors n=%u s=0x%02x a=0x%02x\n\n\n\n\n\n", frameCnt, frameCntTotal, n, status, ksz8851_regrd(REG_RX_ADDR_PTR));
        }
    }
    else
    {
        // no printf inside portDISABLE_INTERRUPTS()
        //MSG("Retr[%u/%u] invalid n=%u s=0x%02x a=%02x\n\n\n\n\n\n", frameCnt, frameCntTotal, n, status, ksz8851_regrd(REG_RX_ADDR_PTR));
    }
    //Release the current error frame from RXQ
    //MSG("clearing\n");
    spi_setbits(REG_RXQ_CMD, RXQ_CMD_FREE_PACKET);
    while ( ksz8851_regrd(REG_RXQ_CMD) & RXQ_CMD_FREE_PACKET ){
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

/* ksz8851RegisterLinkStatusCb() register callback for link status update,
 */
void ksz8851RegisterEvtCb(ksz8851_evt_cb_t evt_cb)
{
    evt_cb_func = evt_cb;
}

