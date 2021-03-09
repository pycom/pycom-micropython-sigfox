/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <string.h>
#include "otplat_radio.h"

#include "pycom_config.h"
#include "mods/modlora.h"

#include <openthread/platform/radio.h>
#include <openthread-core-config.h>
#include <openthread/platform/alarm-milli.h>

#include "esp_wifi.h"
#include "random.h"

/******************************************************************************
 DEFINE PRIVATE CONSTANTS
 ******************************************************************************/
enum {
    IEEE802154_MIN_LENGTH = 5,
    IEEE802154_MAX_LENGTH = 127,
    IEEE802154_ACK_LENGTH = 5,

    IEEE802154_BROADCAST = 0xffff,

    IEEE802154_FRAME_TYPE_ACK = 2 << 0,
    IEEE802154_FRAME_TYPE_MACCMD = 3 << 0,
    IEEE802154_FRAME_TYPE_MASK = 7 << 0,

    IEEE802154_SECURITY_ENABLED = 1 << 3,
    IEEE802154_FRAME_PENDING = 1 << 4,
    IEEE802154_ACK_REQUEST = 1 << 5,
    IEEE802154_PANID_COMPRESSION = 1 << 6,

    IEEE802154_DST_ADDR_NONE = 0 << 2,
    IEEE802154_DST_ADDR_SHORT = 2 << 2,
    IEEE802154_DST_ADDR_EXT = 3 << 2,
    IEEE802154_DST_ADDR_MASK = 3 << 2,

    IEEE802154_SRC_ADDR_NONE = 0 << 6,
    IEEE802154_SRC_ADDR_SHORT = 2 << 6,
    IEEE802154_SRC_ADDR_EXT = 3 << 6,
    IEEE802154_SRC_ADDR_MASK = 3 << 6,

    IEEE802154_DSN_OFFSET = 2,
    IEEE802154_DSTPAN_OFFSET = 3,
    IEEE802154_DSTADDR_OFFSET = 5,

    IEEE802154_SEC_LEVEL_MASK = 7 << 0,

    IEEE802154_KEY_ID_MODE_0 = 0 << 3,
    IEEE802154_KEY_ID_MODE_1 = 1 << 3,
    IEEE802154_KEY_ID_MODE_2 = 2 << 3,
    IEEE802154_KEY_ID_MODE_3 = 3 << 3,
    IEEE802154_KEY_ID_MODE_MASK = 3 << 3,

    IEEE802154_MACCMD_DATA_REQ = 4,
};

#define POSIX_MAX_SRC_MATCH_ENTRIES OPENTHREAD_CONFIG_MAX_CHILDREN

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static otRadioState sState = OT_RADIO_STATE_DISABLED;

static uint8_t sTransmitMessage[OT_RADIO_FRAME_MAX_SIZE];
static uint8_t sReceiveMessage[OT_RADIO_FRAME_MAX_SIZE];
static uint8_t sAckMessage[IEEE802154_ACK_LENGTH + 1];
static otRadioFrame sReceiveFrame;
static otRadioFrame sTransmitFrame;
static otRadioFrame sAckFrame;

static uint8_t sExtendedAddress[OT_EXT_ADDRESS_SIZE];
static uint16_t sShortAddress;
static uint16_t sPanid;

static bool sPromiscuous = false;
static bool sAckWait = false;

static uint8_t sShortAddressMatchTableCount = 0;
static uint8_t sExtAddressMatchTableCount = 0;
static uint16_t sShortAddressMatchTable[POSIX_MAX_SRC_MATCH_ENTRIES];
static otExtAddress sExtAddressMatchTable[POSIX_MAX_SRC_MATCH_ENTRIES];
static bool sSrcMatchEnabled = false;

static int8_t txPower = 14; //dBm

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void radioTransmit(const struct otRadioFrame *pkt);
static void radioSendMessage(otInstance *aInstance);
static void radioSendAck(void);
static void radioProcessFrame(otInstance *aInstance);
void radioReceive(otInstance *aInstance);

static bool findShortAddress(uint16_t aShortAddress);
static bool findExtAddress(const otExtAddress *aExtAddress);
static inline bool isFrameTypeAck(const uint8_t *frame);
static inline bool isFrameTypeMacCmd(const uint8_t *frame);
static inline bool isSecurityEnabled(const uint8_t *frame);
static inline bool isAckRequested(const uint8_t *frame);
static inline bool isPanIdCompressed(const uint8_t *frame);
static inline bool isDataRequestAndHasFramePending(const uint8_t *frame);
static inline uint8_t getDsn(const uint8_t *frame);
static inline otPanId getDstPan(const uint8_t *frame);
static inline otShortAddress getShortAddress(const uint8_t *frame);
static inline void getExtAddress(const uint8_t *frame, otExtAddress *address);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
/**
 * Init openthread radio interface.
 */
void otPlatRadioInit(void) {
    sTransmitFrame.mLength = 0;
    sTransmitFrame.mPsdu = sTransmitMessage;

    sReceiveFrame.mLength = 0;
    sReceiveFrame.mPsdu = sReceiveMessage;

    sAckFrame.mLength = 0;
    sAckFrame.mPsdu = sAckMessage;
}

/**
 * The following are valid radio state transitions:
 *
 *                                    (Radio ON)
 *  +----------+  Enable()  +-------+  Receive() +---------+   Transmit()  +----------+
 *  |          |----------->|       |----------->|         |-------------->|          |
 *  | Disabled |            | Sleep |            | Receive |               | Transmit |
 *  |          |<-----------|       |<-----------|         |<--------------|          |
 *  +----------+  Disable() +-------+   Sleep()  +---------+   Receive()   +----------+
 *                                    (Radio OFF)                 or
 *                                                        signal TransmitDone
 */

/**
 * Get the factory-assigned IEEE EUI-64 for this interface.
 *
 * @param[in]  aInstance   The OpenThread instance structure.
 * @param[out] aIeeeEui64  A pointer to the factory-assigned IEEE EUI-64.
 *
 */
void otPlatRadioGetIeeeEui64(otInstance *aInstance, uint8_t *aIeeeEui64) {
    (void) aInstance;
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioGetIeeeEui64");
    config_get_lpwan_mac(aIeeeEui64);  // get LoRa MAC address
}

/**
 * Set the PAN ID for address filtering.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 * @param[in] aPanId     The IEEE 802.15.4 PAN ID.
 *
 */
void otPlatRadioSetPanId(otInstance *aInstance, uint16_t aPanId) {
    (void) aInstance;
    sPanid = aPanId;
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioSetPanId: 0x%x", sPanid);
}

/**
 * Set the Extended Address for address filtering.
 *
 * @param[in] aInstance    The OpenThread instance structure.
 * @param[in] aExtAddress  A pointer to the IEEE 802.15.4 Extended Address stored in little-endian byte order.
 *
 *
 */
void otPlatRadioSetExtendedAddress(otInstance *aInstance,
        const otExtAddress *aExtAddress) {
    (void) aInstance;

    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioSetExtendedAddress");

    for (size_t i = 0; i < sizeof(sExtendedAddress); i++) {
        sExtendedAddress[i] = aExtAddress->m8[sizeof(sExtendedAddress) - 1 - i];
    }
}

/**
 * Set the Short Address for address filtering.
 *
 * @param[in] aInstance      The OpenThread instance structure.
 * @param[in] aShortAddress  The IEEE 802.15.4 Short Address.
 *
 */
void otPlatRadioSetShortAddress(otInstance *aInstance, uint16_t aShortAddress) {
    (void) aInstance;
    sShortAddress = aShortAddress;
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioSetShortAddress: 0x%x",
            sShortAddress);
}

/**
 * Get current state of the radio.
 *
 * This function is not required by OpenThread. It may be used for debugging and/or application-specific purposes.
 *
 * @note This function may be not implemented. It does not affect OpenThread.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 *
 * @return  Current state of the radio.
 */
otRadioState otPlatRadioGetState(otInstance *aInstance) {
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioGetState %d", sState);
    return sState;
}

/**
 * Enable the radio.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 *
 * @retval OT_ERROR_NONE     Successfully enabled.
 * @retval OT_ERROR_FAILED   The radio could not be enabled.
 */
otError otPlatRadioEnable(otInstance *aInstance) {
    if (!otPlatRadioIsEnabled(aInstance)) {
        sState = OT_RADIO_STATE_SLEEP;
    }
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioEnable");
    return OT_ERROR_NONE;
}

/**
 * Disable the radio.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 *
 * @retval OT_ERROR_NONE  Successfully transitioned to Disabled.
 */
otError otPlatRadioDisable(otInstance *aInstance) {
    if (otPlatRadioIsEnabled(aInstance)) {
        sState = OT_RADIO_STATE_DISABLED;
    }
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioDisable");
    return OT_ERROR_NONE;
}

/**
 * Check whether radio is enabled or not.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 *
 * @returns TRUE if the radio is enabled, FALSE otherwise.
 *
 */
bool otPlatRadioIsEnabled(otInstance *aInstance) {
    (void) aInstance;
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioIsEnabled %d", sState);
    return (sState != OT_RADIO_STATE_DISABLED) ? true : false;
}

/**
 * Transition the radio from Receive to Sleep.
 * Turn off the radio.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 *
 * @retval OT_ERROR_NONE          Successfully transitioned to Sleep.
 * @retval OT_ERROR_BUSY          The radio was transmitting
 * @retval OT_ERROR_INVALID_STATE The radio was disabled
 */
otError otPlatRadioSleep(otInstance *aInstance) {
    (void) aInstance;
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioSleep");

    otError error = OT_ERROR_INVALID_STATE;

    if (sState == OT_RADIO_STATE_SLEEP || sState == OT_RADIO_STATE_RECEIVE) {
        error = OT_ERROR_NONE;
        sState = OT_RADIO_STATE_SLEEP;
    }

    return error;
}

/**
 * Transitioning the radio from Sleep to Receive.
 * Turn on the radio.
 *
 * @param[in]  aInstance  The OpenThread instance structure.
 * @param[in]  aChannel   The channel to use for receiving.
 *
 * @retval OT_ERROR_NONE          Successfully transitioned to Receive.
 * @retval OT_ERROR_INVALID_STATE The radio was disabled or transmitting.
 */
otError otPlatRadioReceive(otInstance *aInstance, uint8_t aChannel) {
    (void) aInstance;

    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otRx s=%d ch=%d", sState, aChannel);

    otError error = OT_ERROR_INVALID_STATE;
    (void) aInstance;

    if (sState != OT_RADIO_STATE_DISABLED) {
        error = OT_ERROR_NONE;
        sState = OT_RADIO_STATE_RECEIVE;
        sAckWait = false;
        sReceiveFrame.mChannel = aChannel;
    }

    return error;
}

/**
 * Enable/Disable source address match feature.
 *
 * The source address match feature controls how the radio layer decides the "frame pending" bit for acks sent in
 * response to data request commands from children.
 *
 * If disabled, the radio layer must set the "frame pending" on all acks to data request commands.
 *
 * If enabled, the radio layer uses the source address match table to determine whether to set or clear the "frame
 * pending" bit in an ack to a data request command.
 *
 * The source address match table provides the list of children for which there is a pending frame. Either a short
 * address or an extended/long address can be added to the source address match table.
 *
 * @param[in]  aInstance   The OpenThread instance structure.
 * @param[in]  aEnable     Enable/disable source address match feature.
 */
void otPlatRadioEnableSrcMatch(otInstance *aInstance, bool aEnable) {
    (void) aInstance;
    sSrcMatchEnabled = aEnable;
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioEnableSrcMatch");
    return;
}

/**
 * Add a short address to the source address match table.
 *
 * @param[in]  aInstance      The OpenThread instance structure.
 * @param[in]  aShortAddress  The short address to be added.
 *
 * @retval OT_ERROR_NONE      Successfully added short address to the source match table.
 * @retval OT_ERROR_NO_BUFS   No available entry in the source match table.
 */
otError otPlatRadioAddSrcMatchShortEntry(otInstance *aInstance,
        const uint16_t aShortAddress) {
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioAddSrcMatchShortEntry");
    otError error = OT_ERROR_NONE;
    otEXPECT_ACTION(
            sShortAddressMatchTableCount
                    < sizeof(sShortAddressMatchTable) / sizeof(uint16_t),
            error = OT_ERROR_NO_BUFS);

    for (uint8_t i = 0; i < sShortAddressMatchTableCount; ++i) {
        otEXPECT_ACTION(sShortAddressMatchTable[i] != aShortAddress, error =
                OT_ERROR_DUPLICATED);
    }

    sShortAddressMatchTable[sShortAddressMatchTableCount++] = aShortAddress;

    exit: (void) aInstance;
    return error;
}

/**
 * Add an extended address to the source address match table.
 *
 * @param[in]  aInstance    The OpenThread instance structure.
 * @param[in]  aExtAddress  The extended address to be added stored in little-endian byte order.
 *
 * @retval OT_ERROR_NONE      Successfully added extended address to the source match table.
 * @retval OT_ERROR_NO_BUFS   No available entry in the source match table.
 */
otError otPlatRadioAddSrcMatchExtEntry(otInstance *aInstance,
        const otExtAddress *aExtAddress) {
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioAddSrcMatchExtEntry");
    otError error = OT_ERROR_NONE;

    otEXPECT_ACTION(
            sExtAddressMatchTableCount
                    < sizeof(sExtAddressMatchTable) / sizeof(otExtAddress),
            error = OT_ERROR_NO_BUFS);

    for (uint8_t i = 0; i < sExtAddressMatchTableCount; ++i) {
        otEXPECT_ACTION(
                memcmp(&sExtAddressMatchTable[i], aExtAddress,
                        sizeof(otExtAddress)), error = OT_ERROR_DUPLICATED);
    }

    sExtAddressMatchTable[sExtAddressMatchTableCount++] = *aExtAddress;

    exit: (void) aInstance;
    return error;
}

/**
 * Remove a short address from the source address match table.
 *
 * @param[in]  aInstance      The OpenThread instance structure.
 * @param[in]  aShortAddress  The short address to be removed.
 *
 * @retval OT_ERROR_NONE        Successfully removed short address from the source match table.
 * @retval OT_ERROR_NO_ADDRESS  The short address is not in source address match table.
 */
otError otPlatRadioClearSrcMatchShortEntry(otInstance *aInstance,
        const uint16_t aShortAddress) {
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioClearSrcMatchShortEntry");
    otError error = OT_ERROR_NOT_FOUND;
    otEXPECT(sShortAddressMatchTableCount > 0);

    for (uint8_t i = 0; i < sShortAddressMatchTableCount; ++i) {
        if (sShortAddressMatchTable[i] == aShortAddress) {
            sShortAddressMatchTable[i] =
                    sShortAddressMatchTable[--sShortAddressMatchTableCount];
            error = OT_ERROR_NONE;
            goto exit;
        }
    }

    exit: (void) aInstance;
    return error;
}

/**
 * Remove an extended address from the source address match table.
 *
 * @param[in]  aInstance    The OpenThread instance structure.
 * @param[in]  aExtAddress  The extended address to be removed stored in little-endian byte order.
 *
 * @retval OT_ERROR_NONE        Successfully removed the extended address from the source match table.
 * @retval OT_ERROR_NO_ADDRESS  The extended address is not in source address match table.
 */
otError otPlatRadioClearSrcMatchExtEntry(otInstance *aInstance,
        const otExtAddress *aExtAddress) {
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioClearSrcMatchExtEntry");
    otError error = OT_ERROR_NOT_FOUND;

    otEXPECT(sExtAddressMatchTableCount > 0);

    for (uint8_t i = 0; i < sExtAddressMatchTableCount; ++i) {
        if (!memcmp(&sExtAddressMatchTable[i], aExtAddress,
                sizeof(otExtAddress))) {
            sExtAddressMatchTable[i] =
                    sExtAddressMatchTable[--sExtAddressMatchTableCount];
            error = OT_ERROR_NONE;
            goto exit;
        }
    }

    exit: (void) aInstance;
    return error;
}

/**
 * Clear all short addresses from the source address match table.
 *
 * @param[in]  aInstance   The OpenThread instance structure.
 *
 */
void otPlatRadioClearSrcMatchShortEntries(otInstance *aInstance) {
    sShortAddressMatchTableCount = 0;
    (void) aInstance;
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioClearSrcMatchShortEntries");
}

/**
 * Clear all the extended/long addresses from source address match table.
 *
 * @param[in]  aInstance   The OpenThread instance structure.
 *
 */
void otPlatRadioClearSrcMatchExtEntries(otInstance *aInstance) {
    sExtAddressMatchTableCount = 0;
    (void) aInstance;
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioClearSrcMatchExtEntries");
}

/**
 * The radio driver calls this method to notify OpenThread of a received frame.
 *
 * @param[in]  aInstance The OpenThread instance structure.
 * @param[in]  aFrame    A pointer to the received frame or NULL if the receive operation failed.
 * @param[in]  aError    OT_ERROR_NONE when successfully received a frame, OT_ERROR_ABORT when reception
 *                       was aborted and a frame was not received, OT_ERROR_NO_BUFS when a frame could not be
 *                       received due to lack of rx buffer space.
 *
 */
//extern void otPlatRadioReceiveDone(otInstance *aInstance, otRadioFrame *aFrame, otError aError);
/**
 * The radio transitions from Transmit to Receive.
 * This method returns a pointer to the transmit buffer.
 *
 * The caller forms the IEEE 802.15.4 frame in this buffer then calls otPlatRadioTransmit() to request transmission.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 *
 * @returns A pointer to the transmit buffer.
 *
 */
otRadioFrame *otPlatRadioGetTransmitBuffer(otInstance *aInstance) {
    (void) aInstance;
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "getTxBuf");
    return &sTransmitFrame;
}

/**
 * This method begins the transmit sequence on the radio.
 *
 * The caller must form the IEEE 802.15.4 frame in the buffer provided by otPlatRadioGetTransmitBuffer() before
 * requesting transmission.  The channel and transmit power are also included in the otRadioFrame structure.
 *
 * The transmit sequence consists of:
 * 1. Transitioning the radio to Transmit from Receive.
 * 2. Transmits the psdu on the given channel and at the given transmit power.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 * @param[in] aFrame     A pointer to the transmitted frame.
 *
 * @retval OT_ERROR_NONE          Successfully transitioned to Transmit.
 * @retval OT_ERROR_INVALID_STATE The radio was not in the Receive state.
 */

otError otPlatRadioTransmit(otInstance *aInstance, otRadioFrame *aFrame) {
    (void) aInstance;

    otError error = OT_ERROR_INVALID_STATE;
    (void) aInstance;
    (void) aFrame;

    if (sState == OT_RADIO_STATE_RECEIVE) {
        error = OT_ERROR_NONE;
        sState = OT_RADIO_STATE_TRANSMIT;
    }

    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otTx s=%d, l=%d, ch=%d", sState,
            aFrame->mLength, aFrame->mChannel);

    return error;
}

/**
 * The radio driver calls this method to notify OpenThread that the transmission has started.
 *
 * @note  This function should be called by the same thread that executes all of the other OpenThread code. It should
 *        not be called by ISR or any other task.
 *
 * @param[in]  aInstance  A pointer to the OpenThread instance structure.
 * @param[in]  aFrame     A pointer to the frame that is being transmitted.
 *
 */
//extern void otPlatRadioTxStarted(otInstance *aInstance, otRadioFrame *aFrame);
/**
 * The radio driver calls this function to notify OpenThread that the transmit operation has completed,
 * providing both the transmitted frame and, if applicable, the received ack frame.
 *
 * @param[in]  aInstance  The OpenThread instance structure.
 * @param[in]  aFrame     A pointer to the frame that was transmitted.
 * @param[in]  aAckFrame  A pointer to the ACK frame, NULL if no ACK was received.
 * @param[in]  aError     OT_ERROR_NONE when the frame was transmitted, OT_ERROR_NO_ACK when the frame was
 *                        transmitted but no ACK was received, OT_ERROR_CHANNEL_ACCESS_FAILURE when the transmission
 *                        could not take place due to activity on the channel, OT_ERROR_ABORT when transmission was
 *                        aborted for other reasons.
 *
 */
//extern void otPlatRadioTxDone(otInstance *aInstance, otRadioFrame *aFrame, otRadioFrame *aAckFrame, otError aError);
/**
 * Get the most recent RSSI measurement.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 *
 * @returns The RSSI in dBm when it is valid.  127 when RSSI is invalid.
 */
int8_t otPlatRadioGetRssi(otInstance *aInstance) {
    (void) aInstance;
    // should be obtained from modlora.c lora_obj.rssi
    int8_t rssi = sReceiveFrame.mInfo.mRxInfo.mRssi;

    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioGetRssi %d", rssi);

    return rssi;
}

/**
 * Get the radio capabilities.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 *
 * @returns The radio capability bit vector. The stack enables or disables some functions based on this value.
 */
otRadioCaps otPlatRadioGetCaps(otInstance *aInstance) {
    (void) aInstance;

    otRadioCaps caps = OT_RADIO_CAPS_NONE;
//    OT_RADIO_CAPS_NONE             = 0, ///< None
//    OT_RADIO_CAPS_ACK_TIMEOUT      = 1, ///< Radio supports AckTime event
//    OT_RADIO_CAPS_ENERGY_SCAN      = 2, ///< Radio supports Energy Scans
//    OT_RADIO_CAPS_TRANSMIT_RETRIES = 4, ///< Radio supports transmission retry logic with collision avoidance (CSMA).
//    OT_RADIO_CAPS_CSMA_BACKOFF     = 8, ///< Radio supports CSMA backoff for frame transmission (but no retry).

    //otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioGetCaps 0x%x", caps);

    return caps;
}

/**
 * Get the radio's transmit power in dBm.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 * @param[out] aPower    The transmit power in dBm.
 *
 * @retval OT_ERROR_NONE             Successfully retrieved the transmit power.
 * @retval OT_ERROR_INVALID_ARGS     @p aPower was NULL.
 * @retval OT_ERROR_NOT_IMPLEMENTED  Transmit power configuration via dBm is not implemented.
 *
 */

otError otPlatRadioGetTransmitPower(otInstance *aInstance, int8_t *aPower) {
    (void) aInstance;

    aPower = &txPower;
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioGetTransmitPower %d dBm",
            *aPower);

    return OT_ERROR_NONE;
}

/**
 * Set the radio's transmit power in dBm.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 * @param[in] aPower     The transmit power in dBm.
 *
 * @retval OT_ERROR_NONE             Successfully set the transmit power.
 * @retval OT_ERROR_NOT_IMPLEMENTED  Transmit power configuration via dBm is not implemented.
 *
 */
otError otPlatRadioSetTransmitPower(otInstance *aInstance, int8_t aPower) {
    (void) aInstance;

    txPower = aPower;
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioSetTransmitPower %d", txPower);
    return OT_ERROR_NONE;
}

/**
 * Get the status of promiscuous mode.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 *
 * @retval true   Promiscuous mode is enabled.
 * @retval false  Promiscuous mode is disabled.
 */
bool otPlatRadioGetPromiscuous(otInstance *aInstance) {
    (void) aInstance;
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioGetPromiscuous %d",
            sPromiscuous);
    return sPromiscuous;
}

/**
 * Enable or disable promiscuous mode.
 *
 * @param[in]  aInstance The OpenThread instance structure.
 * @param[in]  aEnable   A value to enable or disable promiscuous mode.
 */
void otPlatRadioSetPromiscuous(otInstance *aInstance, bool aEnable) {
    (void) aInstance;
    sPromiscuous = aEnable;
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioSetPromiscuous %d",
            sPromiscuous);
}

/**
 * The radio driver calls this method to notify OpenThread diagnostics module that the transmission has completed.
 *
 * @param[in]  aInstance      The OpenThread instance structure.
 * @param[in]  aFrame         A pointer to the frame that was transmitted.
 * @param[in]  aError         OT_ERROR_NONE when the frame was transmitted, OT_ERROR_CHANNEL_ACCESS_FAILURE when the
 *                            transmission could not take place due to activity on the channel, OT_ERROR_ABORT when
 *                            transmission was aborted for other reasons.
 *
 */
//extern void otPlatDiagRadioTransmitDone(otInstance *aInstance, otRadioFrame *aFrame, otError aError);
/**
 * The radio driver calls this method to notify OpenThread diagnostics module of a received frame.
 *
 * @param[in]  aInstance The OpenThread instance structure.
 * @param[in]  aFrame    A pointer to the received frame or NULL if the receive operation failed.
 * @param[in]  aError    OT_ERROR_NONE when successfully received a frame, OT_ERROR_ABORT when reception
 *                       was aborted and a frame was not received, OT_ERROR_NO_BUFS when a frame could not be
 *                       received due to lack of rx buffer space.
 *
 */
//extern void otPlatDiagRadioReceiveDone(otInstance *aInstance, otRadioFrame *aFrame, otError aError);
/**
 * This method begins the energy scan sequence on the radio.
 *
 * @param[in] aInstance      The OpenThread instance structure.
 * @param[in] aScanChannel   The channel to perform the energy scan on.
 * @param[in] aScanDuration  The duration, in milliseconds, for the channel to be scanned.
 *
 * @retval OT_ERROR_NONE             Successfully started scanning the channel.
 * @retval OT_ERROR_NOT_IMPLEMENTED  The radio doesn't support energy scanning.
 */
otError otPlatRadioEnergyScan(otInstance *aInstance, uint8_t aScanChannel,
        uint16_t aScanDuration) {

    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioEnergyScan");
    return OT_ERROR_NOT_IMPLEMENTED;

    (void) aInstance;
    (void) aScanChannel;
    (void) aScanDuration;
    return OT_ERROR_NOT_IMPLEMENTED;
}

/**
 * The radio driver calls this method to notify OpenThread that the energy scan is complete.
 *
 * @param[in]  aInstance           The OpenThread instance structure.
 * @param[in]  aEnergyScanMaxRssi  The maximum RSSI encountered on the scanned channel.
 *
 */
//extern void otPlatRadioEnergyScanDone(otInstance *aInstance, int8_t aEnergyScanMaxRssi){
//}
/**
 * Get the radio receive sensitivity value.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 *
 * @returns The radio receive sensitivity value in dBm.
 */
int8_t otPlatRadioGetReceiveSensitivity(otInstance *aInstance) {
    // funny, for Lora it's -137dBm, but this number is out of int8_t
    int8_t sensitivity = INT8_MIN;  // *124dBm for 125Khz, SF=7 //INT8_MIN;
    //otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatRadioGetReceiveSensitivity %d", sensitivity);
    return sensitivity;
}

// process function to be called from CPU scheduler periodically
void otRadioProcess(otInstance *aInstance) {
    if (sState != OT_RADIO_STATE_DISABLED) {

        radioReceive(aInstance);

        if (sState == OT_RADIO_STATE_TRANSMIT && !sAckWait) {
            radioSendMessage(aInstance);
        }
    }
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

static bool findShortAddress(uint16_t aShortAddress) {
    uint8_t i;

    for (i = 0; i < sShortAddressMatchTableCount; ++i) {
        if (sShortAddressMatchTable[i] == aShortAddress) {
            break;
        }
    }

    return i < sShortAddressMatchTableCount;
}

static bool findExtAddress(const otExtAddress *aExtAddress) {
    uint8_t i;

    for (i = 0; i < sExtAddressMatchTableCount; ++i) {
        if (!memcmp(&sExtAddressMatchTable[i], aExtAddress,
                sizeof(otExtAddress))) {
            break;
        }
    }

    return i < sExtAddressMatchTableCount;
}

static inline bool isFrameTypeAck(const uint8_t *frame) {
    return (frame[0] & IEEE802154_FRAME_TYPE_MASK) == IEEE802154_FRAME_TYPE_ACK;
}

static inline bool isFrameTypeMacCmd(const uint8_t *frame) {
    return (frame[0] & IEEE802154_FRAME_TYPE_MASK)
            == IEEE802154_FRAME_TYPE_MACCMD;
}

static inline bool isSecurityEnabled(const uint8_t *frame) {
    return (frame[0] & IEEE802154_SECURITY_ENABLED) != 0;
}

static inline bool isAckRequested(const uint8_t *frame) {
    return (frame[0] & IEEE802154_ACK_REQUEST) != 0;
}

static inline bool isPanIdCompressed(const uint8_t *frame) {
    return (frame[0] & IEEE802154_PANID_COMPRESSION) != 0;
}

static inline bool isDataRequestAndHasFramePending(const uint8_t *frame) {
    const uint8_t *cur = frame;
    uint8_t securityControl;
    bool isDataRequest = false;
    bool hasFramePending = false;

    // FCF + DSN
    cur += 2 + 1;

    otEXPECT(isFrameTypeMacCmd(frame));

    // Destination PAN + Address
    switch (frame[1] & IEEE802154_DST_ADDR_MASK) {
    case IEEE802154_DST_ADDR_SHORT:
        cur += sizeof(otPanId) + sizeof(otShortAddress);
        break;

    case IEEE802154_DST_ADDR_EXT:
        cur += sizeof(otPanId) + sizeof(otExtAddress);
        break;

    default:
        goto exit;
    }

    // Source PAN + Address
    switch (frame[1] & IEEE802154_SRC_ADDR_MASK) {
    case IEEE802154_SRC_ADDR_SHORT:
        if (!isPanIdCompressed(frame)) {
            cur += sizeof(otPanId);
        }

        if (sSrcMatchEnabled) {
            hasFramePending = findShortAddress(
                    (uint16_t) (cur[1] << 8 | cur[0]));
        }

        cur += sizeof(otShortAddress);
        break;

    case IEEE802154_SRC_ADDR_EXT:
        if (!isPanIdCompressed(frame)) {
            cur += sizeof(otPanId);
        }

        if (sSrcMatchEnabled) {
            hasFramePending = findExtAddress((const otExtAddress *) cur);
        }

        cur += sizeof(otExtAddress);
        break;

    default:
        goto exit;
    }

    // Security Control + Frame Counter + Key Identifier
    if (isSecurityEnabled(frame)) {
        securityControl = *cur;

        if (securityControl & IEEE802154_SEC_LEVEL_MASK) {
            cur += 1 + 4;
        }

        switch (securityControl & IEEE802154_KEY_ID_MODE_MASK) {
        case IEEE802154_KEY_ID_MODE_0:
            cur += 0;
            break;

        case IEEE802154_KEY_ID_MODE_1:
            cur += 1;
            break;

        case IEEE802154_KEY_ID_MODE_2:
            cur += 5;
            break;

        case IEEE802154_KEY_ID_MODE_3:
            cur += 9;
            break;
        }
    }

    // Command ID
    isDataRequest = cur[0] == IEEE802154_MACCMD_DATA_REQ;

    exit: return isDataRequest && hasFramePending;
}

static inline uint8_t getDsn(const uint8_t *frame) {
    return frame[IEEE802154_DSN_OFFSET];
}

static inline otPanId getDstPan(const uint8_t *frame) {
    return (otPanId) ((frame[IEEE802154_DSTPAN_OFFSET + 1] << 8)
            | frame[IEEE802154_DSTPAN_OFFSET]);
}

static inline otShortAddress getShortAddress(const uint8_t *frame) {
    return (otShortAddress) ((frame[IEEE802154_DSTADDR_OFFSET + 1] << 8)
            | frame[IEEE802154_DSTADDR_OFFSET]);
}

static inline void getExtAddress(const uint8_t *frame, otExtAddress *address) {
    size_t i;

    for (i = 0; i < sizeof(otExtAddress); i++) {
        address->m8[i] = frame[IEEE802154_DSTADDR_OFFSET
                + (sizeof(otExtAddress) - 1 - i)];
    }
}

void radioReceive(otInstance *aInstance) {
    bool    isAck;
    ssize_t rval = lora_ot_recv(sReceiveFrame.mPsdu,
            &(sReceiveFrame.mInfo.mRxInfo.mRssi));
    if (rval <= 0)
        return;

    if (otPlatRadioGetPromiscuous(aInstance)) {
        // Timestamp
        sReceiveFrame.mInfo.mRxInfo.mMsec = otPlatAlarmMilliGetNow();
        sReceiveFrame.mInfo.mRxInfo.mUsec = 0; // Don't support microsecond timer for now.
    }

#if OPENTHREAD_CONFIG_ENABLE_TIME_SYNC
    sReceiveFrame.mIeInfo->mTimestamp = otPlatTimeGet();
#endif

    sReceiveFrame.mLength = rval;

    isAck = isFrameTypeAck(sReceiveFrame.mPsdu);

    if (sAckWait && sTransmitFrame.mChannel == sReceiveFrame.mChannel && isAck
            && getDsn(sReceiveFrame.mPsdu) == getDsn(sTransmitFrame.mPsdu)) {
        otPlatLog(OT_LOG_LEVEL_DEBG, 0, "ACK RX");
        sState = OT_RADIO_STATE_RECEIVE;
        sAckWait = false;

        otPlatRadioTxDone(aInstance, &sTransmitFrame, &sReceiveFrame,
                OT_ERROR_NONE);
    } else if ((sState == OT_RADIO_STATE_RECEIVE
            || sState == OT_RADIO_STATE_TRANSMIT)
            //&& (sReceiveFrame.mChannel == sReceiveMessage.mChannel)
            && (!isAck || sPromiscuous)) {
        radioProcessFrame(aInstance);
    }

}

void radioSendMessage(otInstance *aInstance) {
#if OPENTHREAD_CONFIG_HEADER_IE_SUPPORT
    bool notifyFrameUpdated = false;

#if OPENTHREAD_CONFIG_ENABLE_TIME_SYNC
    if (sTransmitFrame.mIeInfo->mTimeIeOffset != 0)
    {
        uint8_t *timeIe = sTransmitFrame.mPsdu + sTransmitFrame.mIeInfo->mTimeIeOffset;
        uint64_t time = (uint64_t)((int64_t)otPlatTimeGet() + sTransmitFrame.mIeInfo->mNetworkTimeOffset);

        *timeIe = sTransmitFrame.mIeInfo->mTimeSyncSeq;

        *(++timeIe) = (uint8_t)(time & 0xff);
        for (uint8_t i = 1; i < sizeof(uint64_t); i++)
        {
            time = time >> 8;
            *(++timeIe) = (uint8_t)(time & 0xff);
        }

        notifyFrameUpdated = true;
    }
#endif // OPENTHREAD_CONFIG_ENABLE_TIME_SYNC

    if (notifyFrameUpdated)
    {
        otPlatRadioFrameUpdated(aInstance, &sTransmitFrame);
    }
#endif // OPENTHREAD_CONFIG_HEADER_IE_SUPPORT

    //sTransmitMessage.mChannel = sTransmitFrame.mChannel;

    otPlatRadioTxStarted(aInstance, &sTransmitFrame);
    radioTransmit(&sTransmitFrame);

    sAckWait = isAckRequested(sTransmitFrame.mPsdu);

    if (!sAckWait) {
        otPlatLog(OT_LOG_LEVEL_DEBG, 0, "ACK no req");
        sState = OT_RADIO_STATE_RECEIVE;

#if OPENTHREAD_ENABLE_DIAG

        if (otPlatDiagModeGet())
        {
            otPlatDiagRadioTransmitDone(aInstance, &sTransmitFrame, OT_ERROR_NONE);
        }
        else
#endif
        {
            otPlatRadioTxDone(aInstance, &sTransmitFrame, NULL, OT_ERROR_NONE);
        }
    } else
        otPlatLog(OT_LOG_LEVEL_DEBG, 0, "ACK req");
}

//void radioTransmit(struct RadioMessage *aMessage, const struct otRadioFrame *aFrame)
void radioTransmit(const struct otRadioFrame *aFrame) {
    lora_ot_send(aFrame->mPsdu, aFrame->mLength);
}

void radioSendAck(void) {
    sAckFrame.mLength = IEEE802154_ACK_LENGTH;
    sAckFrame.mPsdu[0] = IEEE802154_FRAME_TYPE_ACK;

    if (isDataRequestAndHasFramePending(sReceiveFrame.mPsdu)) {
        sAckFrame.mPsdu[0] |= IEEE802154_FRAME_PENDING;
    }

    sAckFrame.mPsdu[1] = 0;
    sAckFrame.mPsdu[2] = getDsn(sReceiveFrame.mPsdu);

    sAckFrame.mChannel = sReceiveFrame.mChannel;

    radioTransmit(&sAckFrame);
}

void radioProcessFrame(otInstance *aInstance) {
    otError error = OT_ERROR_NONE;
    otPanId dstpan;
    otShortAddress short_address;
    otExtAddress ext_address;

    otEXPECT_ACTION(sPromiscuous == false, error = OT_ERROR_NONE);

    switch (sReceiveFrame.mPsdu[1] & IEEE802154_DST_ADDR_MASK) {
    case IEEE802154_DST_ADDR_NONE:
        break;

    case IEEE802154_DST_ADDR_SHORT:
        dstpan = getDstPan(sReceiveFrame.mPsdu);
        short_address = getShortAddress(sReceiveFrame.mPsdu);
        otEXPECT_ACTION(
                (dstpan == IEEE802154_BROADCAST || dstpan == sPanid)
                        && (short_address == IEEE802154_BROADCAST
                                || short_address == sShortAddress), error =
                        OT_ERROR_ABORT);
        break;

    case IEEE802154_DST_ADDR_EXT:
        dstpan = getDstPan(sReceiveFrame.mPsdu);
        getExtAddress(sReceiveFrame.mPsdu, &ext_address);
        otEXPECT_ACTION(
                (dstpan == IEEE802154_BROADCAST || dstpan == sPanid)
                        && memcmp(&ext_address, sExtendedAddress,
                                sizeof(ext_address)) == 0, error =
                        OT_ERROR_ABORT);
        break;

    default:
        error = OT_ERROR_ABORT;
        goto exit;
    }

    //sReceiveFrame.mInfo.mRxInfo.mRssi = -20; // RSSI is already set by lora_ot_rcv function
    sReceiveFrame.mInfo.mRxInfo.mLqi = OT_RADIO_LQI_NONE;

    // generate acknowledgment
    if (isAckRequested(sReceiveFrame.mPsdu)) {
        otPlatLog(OT_LOG_LEVEL_DEBG, 0, "ACK TX");
        radioSendAck();
    }

    exit:

    if (error != OT_ERROR_ABORT) {
#if OPENTHREAD_ENABLE_DIAG
        if (otPlatDiagModeGet())
        {
            otPlatDiagRadioReceiveDone(aInstance, error == OT_ERROR_NONE ? &sReceiveFrame : NULL, error);
        }
        else
#endif
        {
            otPlatRadioReceiveDone(aInstance,
                    error == OT_ERROR_NONE ? &sReceiveFrame : NULL, error);
        }
    }
}
