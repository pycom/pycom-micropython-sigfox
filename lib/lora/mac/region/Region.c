/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech
 ___ _____ _   ___ _  _____ ___  ___  ___ ___
/ __|_   _/_\ / __| |/ / __/ _ \| _ \/ __| __|
\__ \ | |/ _ \ (__| ' <| _| (_) |   / (__| _|
|___/ |_/_/ \_\___|_|\_\_| \___/|_|_\\___|___|
embedded.connectivity.solutions===============

Description: LoRa MAC region implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis ( Semtech ), Gregory Cristian ( Semtech ) and Daniel Jaeckle ( STACKFORCE )
*/
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include "lora/system/timer.h"
#include "lora/mac/LoRaMac.h"
#include "esp_attr.h"


// Regional includes
#include "Region.h"


// Setup regions
#ifdef REGION_AS923
#include "RegionAS923.h"
#define AS923_IS_ACTIVE( )                         else if(region == LORAMAC_REGION_AS923) { return true; }
#define AS923_GET_PHY_PARAM( )                     else if(region == LORAMAC_REGION_AS923) { return RegionAS923GetPhyParam( getPhy ); }
#define AS923_SET_BAND_TX_DONE( )                  else if(region == LORAMAC_REGION_AS923) { RegionAS923SetBandTxDone( txDone );}
#define AS923_INIT_DEFAULTS( )                     else if(region == LORAMAC_REGION_AS923) { RegionAS923InitDefaults( type );}
#define AS923_VERIFY( )                            else if(region == LORAMAC_REGION_AS923) { return RegionAS923Verify( verify, phyAttribute ); }
#define AS923_APPLY_CF_LIST( )                     else if(region == LORAMAC_REGION_AS923) { RegionAS923ApplyCFList( applyCFList );}
#define AS923_CHAN_MASK_SET( )                     else if(region == LORAMAC_REGION_AS923) { return RegionAS923ChanMaskSet( chanMaskSet ); }
#define AS923_ADR_NEXT( )                          else if(region == LORAMAC_REGION_AS923) { return RegionAS923AdrNext( adrNext, drOut, txPowOut, adrAckCounter ); }
#define AS923_COMPUTE_RX_WINDOW_PARAMETERS( )      else if(region == LORAMAC_REGION_AS923) { RegionAS923ComputeRxWindowParameters( datarate, minRxSymbols, rxError, rxConfigParams );}
#define AS923_RX_CONFIG( )                         else if(region == LORAMAC_REGION_AS923) { return RegionAS923RxConfig( rxConfig, datarate ); }
#define AS923_TX_CONFIG( )                         else if(region == LORAMAC_REGION_AS923) { return RegionAS923TxConfig( txConfig, txPower, txTimeOnAir ); }
#define AS923_LINK_ADR_REQ( )                      else if(region == LORAMAC_REGION_AS923) { return RegionAS923LinkAdrReq( linkAdrReq, drOut, txPowOut, nbRepOut, nbBytesParsed ); }
#define AS923_RX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_AS923) { return RegionAS923RxParamSetupReq( rxParamSetupReq ); }
#define AS923_NEW_CHANNEL_REQ( )                   else if(region == LORAMAC_REGION_AS923) { return RegionAS923NewChannelReq( newChannelReq ); }
#define AS923_TX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_AS923) { return RegionAS923TxParamSetupReq( txParamSetupReq ); }
#define AS923_DL_CHANNEL_REQ( )                    else if(region == LORAMAC_REGION_AS923) { return RegionAS923DlChannelReq( dlChannelReq ); }
#define AS923_ALTERNATE_DR( )                      else if(region == LORAMAC_REGION_AS923) { return RegionAS923AlternateDr( alternateDr ); }
#define AS923_CALC_BACKOFF( )                      else if(region == LORAMAC_REGION_AS923) { RegionAS923CalcBackOff( calcBackOff );}
#define AS923_NEXT_CHANNEL( )                      else if(region == LORAMAC_REGION_AS923) { return RegionAS923NextChannel( nextChanParams, channel, time, aggregatedTimeOff ); }
#define AS923_CHANNEL_ADD( )                       else if(region == LORAMAC_REGION_AS923) { return RegionAS923ChannelAdd( channelAdd ); }
#define AS923_CHANNEL_REMOVE( )                    else if(region == LORAMAC_REGION_AS923) { return RegionAS923ChannelsRemove( channelRemove ); }
#define AS923_CHANNEL_MANUAL_ADD( )                else if(region == LORAMAC_REGION_AS923) { return RegionAS923ChannelManualAdd( channelAdd ); }
#define AS923_CHANNEL_MANUAL_REMOVE( )             else if(region == LORAMAC_REGION_AS923) { return RegionAS923ChannelsRemove( channelRemove ); }
#define AS923_SET_CONTINUOUS_WAVE( )               else if(region == LORAMAC_REGION_AS923) { RegionAS923SetContinuousWave( continuousWave );}
#define AS923_APPLY_DR_OFFSET( )                   else if(region == LORAMAC_REGION_AS923) { return RegionAS923ApplyDrOffset( downlinkDwellTime, dr, drOffset ); }
#define AS923_GET_CHANNELS( )                      else if(region == LORAMAC_REGION_AS923) { return RegionAS923GetChannels( channels, size ); }
#define AS923_GET_CHANNEL_MASK( )                  else if(region == LORAMAC_REGION_AS923) { return RegionAS923GetChannelMask( channelmask, size ); }
#define AS923_FORCE_JOIN_DATARATE( )               else if(region == LORAMAC_REGION_AS923) { return RegionAS923ForceJoinDataRate( joinDr, alternateDr ); }
#else
#define AS923_IS_ACTIVE( )
#define AS923_GET_PHY_PARAM( )
#define AS923_SET_BAND_TX_DONE( )
#define AS923_INIT_DEFAULTS( )
#define AS923_VERIFY( )
#define AS923_APPLY_CF_LIST( )
#define AS923_CHAN_MASK_SET( )
#define AS923_ADR_NEXT( )
#define AS923_COMPUTE_RX_WINDOW_PARAMETERS( )
#define AS923_RX_CONFIG( )
#define AS923_TX_CONFIG( )
#define AS923_LINK_ADR_REQ( )
#define AS923_RX_PARAM_SETUP_REQ( )
#define AS923_NEW_CHANNEL_REQ( )
#define AS923_TX_PARAM_SETUP_REQ( )
#define AS923_DL_CHANNEL_REQ( )
#define AS923_ALTERNATE_DR( )
#define AS923_CALC_BACKOFF( )
#define AS923_NEXT_CHANNEL( )
#define AS923_CHANNEL_ADD( )
#define AS923_CHANNEL_REMOVE( )
#define AS923_CHANNEL_MANUAL_ADD( )
#define AS923_CHANNEL_MANUAL_REMOVE( )
#define AS923_SET_CONTINUOUS_WAVE( )
#define AS923_APPLY_DR_OFFSET( )
#define AS923_GET_CHANNELS( )
#define AS923_GET_CHANNEL_MASK( )
#define AS923_FORCE_JOIN_DATARATE( )
#endif

#ifdef REGION_AU915
#include "RegionAU915.h"
#define AU915_IS_ACTIVE( )                         else if(region == LORAMAC_REGION_AU915) { return true; }
#define AU915_GET_PHY_PARAM( )                     else if(region == LORAMAC_REGION_AU915) { return RegionAU915GetPhyParam( getPhy ); }
#define AU915_SET_BAND_TX_DONE( )                  else if(region == LORAMAC_REGION_AU915) { RegionAU915SetBandTxDone( txDone );}
#define AU915_INIT_DEFAULTS( )                     else if(region == LORAMAC_REGION_AU915) { RegionAU915InitDefaults( type );}
#define AU915_VERIFY( )                            else if(region == LORAMAC_REGION_AU915) { return RegionAU915Verify( verify, phyAttribute ); }
#define AU915_APPLY_CF_LIST( )                     else if(region == LORAMAC_REGION_AU915) { RegionAU915ApplyCFList( applyCFList );}
#define AU915_CHAN_MASK_SET( )                     else if(region == LORAMAC_REGION_AU915) { return RegionAU915ChanMaskSet( chanMaskSet ); }
#define AU915_ADR_NEXT( )                          else if(region == LORAMAC_REGION_AU915) { return RegionAU915AdrNext( adrNext, drOut, txPowOut, adrAckCounter ); }
#define AU915_COMPUTE_RX_WINDOW_PARAMETERS( )      else if(region == LORAMAC_REGION_AU915) { RegionAU915ComputeRxWindowParameters( datarate, minRxSymbols, rxError, rxConfigParams );}
#define AU915_RX_CONFIG( )                         else if(region == LORAMAC_REGION_AU915) { return RegionAU915RxConfig( rxConfig, datarate ); }
#define AU915_TX_CONFIG( )                         else if(region == LORAMAC_REGION_AU915) { return RegionAU915TxConfig( txConfig, txPower, txTimeOnAir ); }
#define AU915_LINK_ADR_REQ( )                      else if(region == LORAMAC_REGION_AU915) { return RegionAU915LinkAdrReq( linkAdrReq, drOut, txPowOut, nbRepOut, nbBytesParsed ); }
#define AU915_RX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_AU915) { return RegionAU915RxParamSetupReq( rxParamSetupReq ); }
#define AU915_NEW_CHANNEL_REQ( )                   else if(region == LORAMAC_REGION_AU915) { return RegionAU915NewChannelReq( newChannelReq ); }
#define AU915_TX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_AU915) { return RegionAU915TxParamSetupReq( txParamSetupReq ); }
#define AU915_DL_CHANNEL_REQ( )                    else if(region == LORAMAC_REGION_AU915) { return RegionAU915DlChannelReq( dlChannelReq ); }
#define AU915_ALTERNATE_DR( )                      else if(region == LORAMAC_REGION_AU915) { return RegionAU915AlternateDr( alternateDr ); }
#define AU915_CALC_BACKOFF( )                      else if(region == LORAMAC_REGION_AU915) { RegionAU915CalcBackOff( calcBackOff );}
#define AU915_NEXT_CHANNEL( )                      else if(region == LORAMAC_REGION_AU915) { return RegionAU915NextChannel( nextChanParams, channel, time, aggregatedTimeOff ); }
#define AU915_CHANNEL_ADD( )                       else if(region == LORAMAC_REGION_AU915) { return RegionAU915ChannelAdd( channelAdd ); }
#define AU915_CHANNEL_REMOVE( )                    else if(region == LORAMAC_REGION_AU915) { return RegionAU915ChannelsRemove( channelRemove ); }
#define AU915_CHANNEL_MANUAL_ADD( )                else if(region == LORAMAC_REGION_AU915) { return RegionAU915ChannelManualAdd( channelAdd ); }
#define AU915_CHANNEL_MANUAL_REMOVE( )             else if(region == LORAMAC_REGION_AU915) { return RegionAU915ChannelsManualRemove( channelRemove ); }
#define AU915_SET_CONTINUOUS_WAVE( )               else if(region == LORAMAC_REGION_AU915) { RegionAU915SetContinuousWave( continuousWave );}
#define AU915_APPLY_DR_OFFSET( )                   else if(region == LORAMAC_REGION_AU915) { return RegionAU915ApplyDrOffset( downlinkDwellTime, dr, drOffset ); }
#define AU915_GET_CHANNELS( )                      else if(region == LORAMAC_REGION_AU915) { return RegionAU915GetChannels( channels, size ); }
#define AU915_GET_CHANNEL_MASK( )                  else if(region == LORAMAC_REGION_AU915) { return RegionAU915GetChannelMask( channelmask, size ); }
#define AU915_GET_CHANNEL_MASK_REMAINING( )        else if(region == LORAMAC_REGION_AU915) { return RegionAU915GetChannelMaskRemaining( channelmask, size ); }
#define AU915_FORCE_JOIN_DATARATE( )               else if(region == LORAMAC_REGION_AU915) { return RegionAU915ForceJoinDataRate( joinDr, alternateDr ); }
#else
#define AU915_IS_ACTIVE( )
#define AU915_GET_PHY_PARAM( )
#define AU915_SET_BAND_TX_DONE( )
#define AU915_INIT_DEFAULTS( )
#define AU915_VERIFY( )
#define AU915_APPLY_CF_LIST( )
#define AU915_CHAN_MASK_SET( )
#define AU915_ADR_NEXT( )
#define AU915_COMPUTE_RX_WINDOW_PARAMETERS( )
#define AU915_RX_CONFIG( )
#define AU915_TX_CONFIG( )
#define AU915_LINK_ADR_REQ( )
#define AU915_RX_PARAM_SETUP_REQ( )
#define AU915_NEW_CHANNEL_REQ( )
#define AU915_TX_PARAM_SETUP_REQ( )
#define AU915_DL_CHANNEL_REQ( )
#define AU915_ALTERNATE_DR( )
#define AU915_CALC_BACKOFF( )
#define AU915_NEXT_CHANNEL( )
#define AU915_CHANNEL_ADD( )
#define AU915_CHANNEL_REMOVE( )
#define AU915_CHANNEL_MANUAL_ADD( )
#define AU915_CHANNEL_MANUAL_REMOVE( )
#define AU915_SET_CONTINUOUS_WAVE( )
#define AU915_APPLY_DR_OFFSET( )
#define AU915_GET_CHANNELS( )
#define AU915_GET_CHANNEL_MASK( )
#define AU915_GET_CHANNEL_MASK_REMAINING( )
#define AU915_FORCE_JOIN_DATARATE( )
#endif

#ifdef REGION_CN470
#include "RegionCN470.h"
#define CN470_IS_ACTIVE( )                         else if(region == LORAMAC_REGION_CN470) { return true; }
#define CN470_GET_PHY_PARAM( )                     else if(region == LORAMAC_REGION_CN470) { return RegionCN470GetPhyParam( getPhy ); }
#define CN470_SET_BAND_TX_DONE( )                  else if(region == LORAMAC_REGION_CN470) { RegionCN470SetBandTxDone( txDone );}
#define CN470_INIT_DEFAULTS( )                     else if(region == LORAMAC_REGION_CN470) { RegionCN470InitDefaults( type );}
#define CN470_VERIFY( )                            else if(region == LORAMAC_REGION_CN470) { return RegionCN470Verify( verify, phyAttribute ); }
#define CN470_APPLY_CF_LIST( )                     else if(region == LORAMAC_REGION_CN470) { RegionCN470ApplyCFList( applyCFList );}
#define CN470_CHAN_MASK_SET( )                     else if(region == LORAMAC_REGION_CN470) { return RegionCN470ChanMaskSet( chanMaskSet ); }
#define CN470_ADR_NEXT( )                          else if(region == LORAMAC_REGION_CN470) { return RegionCN470AdrNext( adrNext, drOut, txPowOut, adrAckCounter ); }
#define CN470_COMPUTE_RX_WINDOW_PARAMETERS( )      else if(region == LORAMAC_REGION_CN470) { RegionCN470ComputeRxWindowParameters( datarate, minRxSymbols, rxError, rxConfigParams );}
#define CN470_RX_CONFIG( )                         else if(region == LORAMAC_REGION_CN470) { return RegionCN470RxConfig( rxConfig, datarate ); }
#define CN470_TX_CONFIG( )                         else if(region == LORAMAC_REGION_CN470) { return RegionCN470TxConfig( txConfig, txPower, txTimeOnAir ); }
#define CN470_LINK_ADR_REQ( )                      else if(region == LORAMAC_REGION_CN470) { return RegionCN470LinkAdrReq( linkAdrReq, drOut, txPowOut, nbRepOut, nbBytesParsed ); }
#define CN470_RX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_CN470) { return RegionCN470RxParamSetupReq( rxParamSetupReq ); }
#define CN470_NEW_CHANNEL_REQ( )                   else if(region == LORAMAC_REGION_CN470) { return RegionCN470NewChannelReq( newChannelReq ); }
#define CN470_TX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_CN470) { return RegionCN470TxParamSetupReq( txParamSetupReq ); }
#define CN470_DL_CHANNEL_REQ( )                    else if(region == LORAMAC_REGION_CN470) { return RegionCN470DlChannelReq( dlChannelReq ); }
#define CN470_ALTERNATE_DR( )                      else if(region == LORAMAC_REGION_CN470) { return RegionCN470AlternateDr( alternateDr ); }
#define CN470_CALC_BACKOFF( )                      else if(region == LORAMAC_REGION_CN470) { RegionCN470CalcBackOff( calcBackOff );}
#define CN470_NEXT_CHANNEL( )                      else if(region == LORAMAC_REGION_CN470) { return RegionCN470NextChannel( nextChanParams, channel, time, aggregatedTimeOff ); }
#define CN470_CHANNEL_ADD( )                       else if(region == LORAMAC_REGION_CN470) { return RegionCN470ChannelAdd( channelAdd ); }
#define CN470_CHANNEL_REMOVE( )                    else if(region == LORAMAC_REGION_CN470) { return RegionCN470ChannelsRemove( channelRemove ); }
#define CN470_CHANNEL_MANUAL_ADD( )                else if(region == LORAMAC_REGION_CN470) { return RegionCN470ChannelManualAdd( channelAdd ); }
#define CN470_CHANNEL_MANUAL_REMOVE( )             else if(region == LORAMAC_REGION_CN470) { return RegionCN470ChannelsRemove( channelRemove ); }
#define CN470_SET_CONTINUOUS_WAVE( )               else if(region == LORAMAC_REGION_CN470) { RegionCN470SetContinuousWave( continuousWave );}
#define CN470_APPLY_DR_OFFSET( )                   else if(region == LORAMAC_REGION_CN470) { return RegionCN470ApplyDrOffset( downlinkDwellTime, dr, drOffset ); }
#define CN470_FORCE_JOIN_DATARATE( )               else if(region == LORAMAC_REGION_CN470) { return RegionCN470ForceJoinDataRate( joinDr, alternateDr ); }
#else
#define CN470_IS_ACTIVE( )
#define CN470_GET_PHY_PARAM( )
#define CN470_SET_BAND_TX_DONE( )
#define CN470_INIT_DEFAULTS( )
#define CN470_VERIFY( )
#define CN470_APPLY_CF_LIST( )
#define CN470_CHAN_MASK_SET( )
#define CN470_ADR_NEXT( )
#define CN470_COMPUTE_RX_WINDOW_PARAMETERS( )
#define CN470_RX_CONFIG( )
#define CN470_TX_CONFIG( )
#define CN470_LINK_ADR_REQ( )
#define CN470_RX_PARAM_SETUP_REQ( )
#define CN470_NEW_CHANNEL_REQ( )
#define CN470_TX_PARAM_SETUP_REQ( )
#define CN470_DL_CHANNEL_REQ( )
#define CN470_ALTERNATE_DR( )
#define CN470_CALC_BACKOFF( )
#define CN470_NEXT_CHANNEL( )
#define CN470_CHANNEL_ADD( )
#define CN470_CHANNEL_REMOVE( )
#define CN470_CHANNEL_MANUAL_ADD( )
#define CN470_CHANNEL_MANUAL_REMOVE( )
#define CN470_SET_CONTINUOUS_WAVE( )
#define CN470_APPLY_DR_OFFSET( )
#define CN470_FORCE_JOIN_DATARATE( )
#endif

#ifdef REGION_CN779
#include "RegionCN779.h"
#define CN779_IS_ACTIVE( )                         else if(region == LORAMAC_REGION_CN779) { return true; }
#define CN779_GET_PHY_PARAM( )                     else if(region == LORAMAC_REGION_CN779) { return RegionCN779GetPhyParam( getPhy ); }
#define CN779_SET_BAND_TX_DONE( )                  else if(region == LORAMAC_REGION_CN779) { RegionCN779SetBandTxDone( txDone );}
#define CN779_INIT_DEFAULTS( )                     else if(region == LORAMAC_REGION_CN779) { RegionCN779InitDefaults( type );}
#define CN779_VERIFY( )                            else if(region == LORAMAC_REGION_CN779) { return RegionCN779Verify( verify, phyAttribute ); }
#define CN779_APPLY_CF_LIST( )                     else if(region == LORAMAC_REGION_CN779) { RegionCN779ApplyCFList( applyCFList );}
#define CN779_CHAN_MASK_SET( )                     else if(region == LORAMAC_REGION_CN779) { return RegionCN779ChanMaskSet( chanMaskSet ); }
#define CN779_ADR_NEXT( )                          else if(region == LORAMAC_REGION_CN779) { return RegionCN779AdrNext( adrNext, drOut, txPowOut, adrAckCounter ); }
#define CN779_COMPUTE_RX_WINDOW_PARAMETERS( )      else if(region == LORAMAC_REGION_CN779) { RegionCN779ComputeRxWindowParameters( datarate, minRxSymbols, rxError, rxConfigParams );}
#define CN779_RX_CONFIG( )                         else if(region == LORAMAC_REGION_CN779) { return RegionCN779RxConfig( rxConfig, datarate ); }
#define CN779_TX_CONFIG( )                         else if(region == LORAMAC_REGION_CN779) { return RegionCN779TxConfig( txConfig, txPower, txTimeOnAir ); }
#define CN779_LINK_ADR_REQ( )                      else if(region == LORAMAC_REGION_CN779) { return RegionCN779LinkAdrReq( linkAdrReq, drOut, txPowOut, nbRepOut, nbBytesParsed ); }
#define CN779_RX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_CN779) { return RegionCN779RxParamSetupReq( rxParamSetupReq ); }
#define CN779_NEW_CHANNEL_REQ( )                   else if(region == LORAMAC_REGION_CN779) { return RegionCN779NewChannelReq( newChannelReq ); }
#define CN779_TX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_CN779) { return RegionCN779TxParamSetupReq( txParamSetupReq ); }
#define CN779_DL_CHANNEL_REQ( )                    else if(region == LORAMAC_REGION_CN779) { return RegionCN779DlChannelReq( dlChannelReq ); }
#define CN779_ALTERNATE_DR( )                      else if(region == LORAMAC_REGION_CN779) { return RegionCN779AlternateDr( alternateDr ); }
#define CN779_CALC_BACKOFF( )                      else if(region == LORAMAC_REGION_CN779) { RegionCN779CalcBackOff( calcBackOff );}
#define CN779_NEXT_CHANNEL( )                      else if(region == LORAMAC_REGION_CN779) { return RegionCN779NextChannel( nextChanParams, channel, time, aggregatedTimeOff ); }
#define CN779_CHANNEL_ADD( )                       else if(region == LORAMAC_REGION_CN779) { return RegionCN779ChannelAdd( channelAdd ); }
#define CN779_CHANNEL_REMOVE( )                    else if(region == LORAMAC_REGION_CN779) { return RegionCN779ChannelsRemove( channelRemove ); }
#define CN779_SET_CONTINUOUS_WAVE( )               else if(region == LORAMAC_REGION_CN779) { RegionCN779SetContinuousWave( continuousWave );}
#define CN779_APPLY_DR_OFFSET( )                   else if(region == LORAMAC_REGION_CN779) { return RegionCN779ApplyDrOffset( downlinkDwellTime, dr, drOffset ); }
#else
#define CN779_IS_ACTIVE( )
#define CN779_GET_PHY_PARAM( )
#define CN779_SET_BAND_TX_DONE( )
#define CN779_INIT_DEFAULTS( )
#define CN779_VERIFY( )
#define CN779_APPLY_CF_LIST( )
#define CN779_CHAN_MASK_SET( )
#define CN779_ADR_NEXT( )
#define CN779_COMPUTE_RX_WINDOW_PARAMETERS( )
#define CN779_RX_CONFIG( )
#define CN779_TX_CONFIG( )
#define CN779_LINK_ADR_REQ( )
#define CN779_RX_PARAM_SETUP_REQ( )
#define CN779_NEW_CHANNEL_REQ( )
#define CN779_TX_PARAM_SETUP_REQ( )
#define CN779_DL_CHANNEL_REQ( )
#define CN779_ALTERNATE_DR( )
#define CN779_CALC_BACKOFF( )
#define CN779_NEXT_CHANNEL( )
#define CN779_CHANNEL_ADD( )
#define CN779_CHANNEL_REMOVE( )
#define CN779_SET_CONTINUOUS_WAVE( )
#define CN779_APPLY_DR_OFFSET( )
#endif

#ifdef REGION_EU433
#include "RegionEU433.h"
#define EU433_IS_ACTIVE( )                         else if(region == LORAMAC_REGION_EU433) { return true; }
#define EU433_GET_PHY_PARAM( )                     else if(region == LORAMAC_REGION_EU433) { return RegionEU433GetPhyParam( getPhy ); }
#define EU433_SET_BAND_TX_DONE( )                  else if(region == LORAMAC_REGION_EU433) { RegionEU433SetBandTxDone( txDone );}
#define EU433_INIT_DEFAULTS( )                     else if(region == LORAMAC_REGION_EU433) { RegionEU433InitDefaults( type );}
#define EU433_VERIFY( )                            else if(region == LORAMAC_REGION_EU433) { return RegionEU433Verify( verify, phyAttribute ); }
#define EU433_APPLY_CF_LIST( )                     else if(region == LORAMAC_REGION_EU433) { RegionEU433ApplyCFList( applyCFList );}
#define EU433_CHAN_MASK_SET( )                     else if(region == LORAMAC_REGION_EU433) { return RegionEU433ChanMaskSet( chanMaskSet ); }
#define EU433_ADR_NEXT( )                          else if(region == LORAMAC_REGION_EU433) { return RegionEU433AdrNext( adrNext, drOut, txPowOut, adrAckCounter ); }
#define EU433_COMPUTE_RX_WINDOW_PARAMETERS( )      else if(region == LORAMAC_REGION_EU433) { RegionEU433ComputeRxWindowParameters( datarate, minRxSymbols, rxError, rxConfigParams );}
#define EU433_RX_CONFIG( )                         else if(region == LORAMAC_REGION_EU433) { return RegionEU433RxConfig( rxConfig, datarate ); }
#define EU433_TX_CONFIG( )                         else if(region == LORAMAC_REGION_EU433) { return RegionEU433TxConfig( txConfig, txPower, txTimeOnAir ); }
#define EU433_LINK_ADR_REQ( )                      else if(region == LORAMAC_REGION_EU433) { return RegionEU433LinkAdrReq( linkAdrReq, drOut, txPowOut, nbRepOut, nbBytesParsed ); }
#define EU433_RX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_EU433) { return RegionEU433RxParamSetupReq( rxParamSetupReq ); }
#define EU433_NEW_CHANNEL_REQ( )                   else if(region == LORAMAC_REGION_EU433) { return RegionEU433NewChannelReq( newChannelReq ); }
#define EU433_TX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_EU433) { return RegionEU433TxParamSetupReq( txParamSetupReq ); }
#define EU433_DL_CHANNEL_REQ( )                    else if(region == LORAMAC_REGION_EU433) { return RegionEU433DlChannelReq( dlChannelReq ); }
#define EU433_ALTERNATE_DR( )                      else if(region == LORAMAC_REGION_EU433) { return RegionEU433AlternateDr( alternateDr ); }
#define EU433_CALC_BACKOFF( )                      else if(region == LORAMAC_REGION_EU433) { RegionEU433CalcBackOff( calcBackOff );}
#define EU433_NEXT_CHANNEL( )                      else if(region == LORAMAC_REGION_EU433) { return RegionEU433NextChannel( nextChanParams, channel, time, aggregatedTimeOff ); }
#define EU433_CHANNEL_ADD( )                       else if(region == LORAMAC_REGION_EU433) { return RegionEU433ChannelAdd( channelAdd ); }
#define EU433_CHANNEL_REMOVE( )                    else if(region == LORAMAC_REGION_EU433) { return RegionEU433ChannelsRemove( channelRemove ); }
#define EU433_CHANNEL_MANUAL_ADD( )                EU433_CASE { return RegionEU433ChannelManualAdd( channelAdd ); }
#define EU433_CHANNEL_MANUAL_REMOVE( )             EU433_CASE { return RegionEU433ChannelsRemove( channelRemove ); }
#define EU433_SET_CONTINUOUS_WAVE( )               else if(region == LORAMAC_REGION_EU433) { RegionEU433SetContinuousWave( continuousWave );}
#define EU433_APPLY_DR_OFFSET( )                   else if(region == LORAMAC_REGION_EU433) { return RegionEU433ApplyDrOffset( downlinkDwellTime, dr, drOffset ); }
#define EU433_FORCE_JOIN_DATARATE( )               EU433_CASE { return RegionEU433ForceJoinDataRate( joinDr, alternateDr ); }
#else
#define EU433_IS_ACTIVE( )
#define EU433_GET_PHY_PARAM( )
#define EU433_SET_BAND_TX_DONE( )
#define EU433_INIT_DEFAULTS( )
#define EU433_VERIFY( )
#define EU433_APPLY_CF_LIST( )
#define EU433_CHAN_MASK_SET( )
#define EU433_ADR_NEXT( )
#define EU433_COMPUTE_RX_WINDOW_PARAMETERS( )
#define EU433_RX_CONFIG( )
#define EU433_TX_CONFIG( )
#define EU433_LINK_ADR_REQ( )
#define EU433_RX_PARAM_SETUP_REQ( )
#define EU433_NEW_CHANNEL_REQ( )
#define EU433_TX_PARAM_SETUP_REQ( )
#define EU433_DL_CHANNEL_REQ( )
#define EU433_ALTERNATE_DR( )
#define EU433_CALC_BACKOFF( )
#define EU433_NEXT_CHANNEL( )
#define EU433_CHANNEL_ADD( )
#define EU433_CHANNEL_REMOVE( )
#define EU433_CHANNEL_MANUAL_ADD( )
#define EU433_CHANNEL_MANUAL_REMOVE( )
#define EU433_SET_CONTINUOUS_WAVE( )
#define EU433_APPLY_DR_OFFSET( )
#define EU433_FORCE_JOIN_DATARATE( )
#endif

#ifdef REGION_EU868
#include "RegionEU868.h"
#define EU868_IS_ACTIVE( )                         else if(region == LORAMAC_REGION_EU868) { return true; }
#define EU868_GET_PHY_PARAM( )                     else if(region == LORAMAC_REGION_EU868) { return RegionEU868GetPhyParam( getPhy ); }
#define EU868_SET_BAND_TX_DONE( )                  else if(region == LORAMAC_REGION_EU868) { RegionEU868SetBandTxDone( txDone );}
#define EU868_INIT_DEFAULTS( )                     else if(region == LORAMAC_REGION_EU868) { RegionEU868InitDefaults( type );}
#define EU868_VERIFY( )                            else if(region == LORAMAC_REGION_EU868) { return RegionEU868Verify( verify, phyAttribute ); }
#define EU868_APPLY_CF_LIST( )                     else if(region == LORAMAC_REGION_EU868) { RegionEU868ApplyCFList( applyCFList );}
#define EU868_CHAN_MASK_SET( )                     else if(region == LORAMAC_REGION_EU868) { return RegionEU868ChanMaskSet( chanMaskSet ); }
#define EU868_ADR_NEXT( )                          else if(region == LORAMAC_REGION_EU868) { return RegionEU868AdrNext( adrNext, drOut, txPowOut, adrAckCounter ); }
#define EU868_COMPUTE_RX_WINDOW_PARAMETERS( )      else if(region == LORAMAC_REGION_EU868) { RegionEU868ComputeRxWindowParameters( datarate, minRxSymbols, rxError, rxConfigParams );}
#define EU868_RX_CONFIG( )                         else if(region == LORAMAC_REGION_EU868) { return RegionEU868RxConfig( rxConfig, datarate ); }
#define EU868_TX_CONFIG( )                         else if(region == LORAMAC_REGION_EU868) { return RegionEU868TxConfig( txConfig, txPower, txTimeOnAir ); }
#define EU868_LINK_ADR_REQ( )                      else if(region == LORAMAC_REGION_EU868) { return RegionEU868LinkAdrReq( linkAdrReq, drOut, txPowOut, nbRepOut, nbBytesParsed ); }
#define EU868_RX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_EU868) { return RegionEU868RxParamSetupReq( rxParamSetupReq ); }
#define EU868_NEW_CHANNEL_REQ( )                   else if(region == LORAMAC_REGION_EU868) { return RegionEU868NewChannelReq( newChannelReq ); }
#define EU868_TX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_EU868) { return RegionEU868TxParamSetupReq( txParamSetupReq ); }
#define EU868_DL_CHANNEL_REQ( )                    else if(region == LORAMAC_REGION_EU868) { return RegionEU868DlChannelReq( dlChannelReq ); }
#define EU868_ALTERNATE_DR( )                      else if(region == LORAMAC_REGION_EU868) { return RegionEU868AlternateDr( alternateDr ); }
#define EU868_CALC_BACKOFF( )                      else if(region == LORAMAC_REGION_EU868) { RegionEU868CalcBackOff( calcBackOff );}
#define EU868_NEXT_CHANNEL( )                      else if(region == LORAMAC_REGION_EU868) { return RegionEU868NextChannel( nextChanParams, channel, time, aggregatedTimeOff ); }
#define EU868_CHANNEL_ADD( )                       else if(region == LORAMAC_REGION_EU868) { return RegionEU868ChannelAdd( channelAdd ); }
#define EU868_CHANNEL_REMOVE( )                    else if(region == LORAMAC_REGION_EU868) { return RegionEU868ChannelsRemove( channelRemove ); }
#define EU868_CHANNEL_MANUAL_ADD( )                else if(region == LORAMAC_REGION_EU868) { return RegionEU868ChannelManualAdd( channelAdd ); }
#define EU868_CHANNEL_MANUAL_REMOVE( )             else if(region == LORAMAC_REGION_EU868) { return RegionEU868ChannelsRemove( channelRemove ); }
#define EU868_SET_CONTINUOUS_WAVE( )               else if(region == LORAMAC_REGION_EU868) { RegionEU868SetContinuousWave( continuousWave );}
#define EU868_APPLY_DR_OFFSET( )                   else if(region == LORAMAC_REGION_EU868) { return RegionEU868ApplyDrOffset( downlinkDwellTime, dr, drOffset ); }
#define EU868_GET_CHANNELS( )                      else if(region == LORAMAC_REGION_EU868) { return RegionEU868GetChannels( channels, size ); }
#define EU868_GET_CHANNEL_MASK( )                  else if(region == LORAMAC_REGION_EU868) { return RegionEU868GetChannelMask( channelmask, size ); }
#define EU868_FORCE_JOIN_DATARATE( )               else if(region == LORAMAC_REGION_EU868) { return RegionEU868ForceJoinDataRate( joinDr, alternateDr ); }
#else
#define EU868_IS_ACTIVE( )
#define EU868_GET_PHY_PARAM( )
#define EU868_SET_BAND_TX_DONE( )
#define EU868_INIT_DEFAULTS( )
#define EU868_VERIFY( )
#define EU868_APPLY_CF_LIST( )
#define EU868_CHAN_MASK_SET( )
#define EU868_ADR_NEXT( )
#define EU868_COMPUTE_RX_WINDOW_PARAMETERS( )
#define EU868_RX_CONFIG( )
#define EU868_TX_CONFIG( )
#define EU868_LINK_ADR_REQ( )
#define EU868_RX_PARAM_SETUP_REQ( )
#define EU868_NEW_CHANNEL_REQ( )
#define EU868_TX_PARAM_SETUP_REQ( )
#define EU868_DL_CHANNEL_REQ( )
#define EU868_ALTERNATE_DR( )
#define EU868_CALC_BACKOFF( )
#define EU868_NEXT_CHANNEL( )
#define EU868_CHANNEL_ADD( )
#define EU868_CHANNEL_REMOVE( )
#define EU868_CHANNEL_MANUAL_ADD( )
#define EU868_CHANNEL_MANUAL_REMOVE( )
#define EU868_SET_CONTINUOUS_WAVE( )
#define EU868_APPLY_DR_OFFSET( )
#define EU868_GET_CHANNELS( )
#define EU868_GET_CHANNEL_MASK( )
#define EU868_FORCE_JOIN_DATARATE( )
#endif

#ifdef REGION_KR920
#include "RegionKR920.h"
#define KR920_IS_ACTIVE( )                         else if(region == LORAMAC_REGION_KR920) { return true; }
#define KR920_GET_PHY_PARAM( )                     else if(region == LORAMAC_REGION_KR920) { return RegionKR920GetPhyParam( getPhy  ); }
#define KR920_SET_BAND_TX_DONE( )                  else if(region == LORAMAC_REGION_KR920) { RegionKR920SetBandTxDone( txDone );}
#define KR920_INIT_DEFAULTS( )                     else if(region == LORAMAC_REGION_KR920) { RegionKR920InitDefaults( type );}
#define KR920_VERIFY( )                            else if(region == LORAMAC_REGION_KR920) { return RegionKR920Verify( verify, phyAttribute ); }
#define KR920_APPLY_CF_LIST( )                     else if(region == LORAMAC_REGION_KR920) { RegionKR920ApplyCFList( applyCFList );}
#define KR920_CHAN_MASK_SET( )                     else if(region == LORAMAC_REGION_KR920) { return RegionKR920ChanMaskSet( chanMaskSet ); }
#define KR920_ADR_NEXT( )                          else if(region == LORAMAC_REGION_KR920) { return RegionKR920AdrNext( adrNext, drOut, txPowOut, adrAckCounter ); }
#define KR920_COMPUTE_RX_WINDOW_PARAMETERS( )      else if(region == LORAMAC_REGION_KR920) { RegionKR920ComputeRxWindowParameters( datarate, minRxSymbols, rxError, rxConfigParams );}
#define KR920_RX_CONFIG( )                         else if(region == LORAMAC_REGION_KR920) { return RegionKR920RxConfig( rxConfig, datarate ); }
#define KR920_TX_CONFIG( )                         else if(region == LORAMAC_REGION_KR920) { return RegionKR920TxConfig( txConfig, txPower, txTimeOnAir ); }
#define KR920_LINK_ADR_REQ( )                      else if(region == LORAMAC_REGION_KR920) { return RegionKR920LinkAdrReq( linkAdrReq, drOut, txPowOut, nbRepOut, nbBytesParsed ); }
#define KR920_RX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_KR920) { return RegionKR920RxParamSetupReq( rxParamSetupReq ); }
#define KR920_NEW_CHANNEL_REQ( )                   else if(region == LORAMAC_REGION_KR920) { return RegionKR920NewChannelReq( newChannelReq ); }
#define KR920_TX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_KR920) { return RegionKR920TxParamSetupReq( txParamSetupReq ); }
#define KR920_DL_CHANNEL_REQ( )                    else if(region == LORAMAC_REGION_KR920) { return RegionKR920DlChannelReq( dlChannelReq ); }
#define KR920_ALTERNATE_DR( )                      else if(region == LORAMAC_REGION_KR920) { return RegionKR920AlternateDr( alternateDr ); }
#define KR920_CALC_BACKOFF( )                      else if(region == LORAMAC_REGION_KR920) { RegionKR920CalcBackOff( calcBackOff );}
#define KR920_NEXT_CHANNEL( )                      else if(region == LORAMAC_REGION_KR920) { return RegionKR920NextChannel( nextChanParams, channel, time, aggregatedTimeOff ); }
#define KR920_CHANNEL_ADD( )                       else if(region == LORAMAC_REGION_KR920) { return RegionKR920ChannelAdd( channelAdd ); }
#define KR920_CHANNEL_REMOVE( )                    else if(region == LORAMAC_REGION_KR920) { return RegionKR920ChannelsRemove( channelRemove ); }
#define KR920_SET_CONTINUOUS_WAVE( )               else if(region == LORAMAC_REGION_KR920) { RegionKR920SetContinuousWave( continuousWave );}
#define KR920_APPLY_DR_OFFSET( )                   else if(region == LORAMAC_REGION_KR920) { return RegionKR920ApplyDrOffset( downlinkDwellTime, dr, drOffset ); }
#else
#define KR920_IS_ACTIVE( )
#define KR920_GET_PHY_PARAM( )
#define KR920_SET_BAND_TX_DONE( )
#define KR920_INIT_DEFAULTS( )
#define KR920_VERIFY( )
#define KR920_APPLY_CF_LIST( )
#define KR920_CHAN_MASK_SET( )
#define KR920_ADR_NEXT( )
#define KR920_COMPUTE_RX_WINDOW_PARAMETERS( )
#define KR920_RX_CONFIG( )
#define KR920_TX_CONFIG( )
#define KR920_LINK_ADR_REQ( )
#define KR920_RX_PARAM_SETUP_REQ( )
#define KR920_NEW_CHANNEL_REQ( )
#define KR920_TX_PARAM_SETUP_REQ( )
#define KR920_DL_CHANNEL_REQ( )
#define KR920_ALTERNATE_DR( )
#define KR920_CALC_BACKOFF( )
#define KR920_NEXT_CHANNEL( )
#define KR920_CHANNEL_ADD( )
#define KR920_CHANNEL_REMOVE( )
#define KR920_SET_CONTINUOUS_WAVE( )
#define KR920_APPLY_DR_OFFSET( )
#endif

#ifdef REGION_IN865
#include "RegionIN865.h"
#define IN865_IS_ACTIVE( )                        else if(region == LORAMAC_REGION_IN865) { return true; }
#define IN865_GET_PHY_PARAM( )                    else if(region == LORAMAC_REGION_IN865) { return RegionIN865GetPhyParam( getPhy ); }
#define IN865_SET_BAND_TX_DONE( )                 else if(region == LORAMAC_REGION_IN865) { RegionIN865SetBandTxDone( txDone );}
#define IN865_INIT_DEFAULTS( )                    else if(region == LORAMAC_REGION_IN865) { RegionIN865InitDefaults( type );}
#define IN865_VERIFY( )                           else if(region == LORAMAC_REGION_IN865) { return RegionIN865Verify( verify, phyAttribute ); }
#define IN865_APPLY_CF_LIST( )                    else if(region == LORAMAC_REGION_IN865) { RegionIN865ApplyCFList( applyCFList );}
#define IN865_CHAN_MASK_SET( )                    else if(region == LORAMAC_REGION_IN865) { return RegionIN865ChanMaskSet( chanMaskSet ); }
#define IN865_ADR_NEXT( )                         else if(region == LORAMAC_REGION_IN865) { return RegionIN865AdrNext( adrNext, drOut, txPowOut, adrAckCounter ); }
#define IN865_COMPUTE_RX_WINDOW_PARAMETERS( )     else if(region == LORAMAC_REGION_IN865) { RegionIN865ComputeRxWindowParameters( datarate, minRxSymbols, rxError, rxConfigParams );}
#define IN865_RX_CONFIG( )                        else if(region == LORAMAC_REGION_IN865) { return RegionIN865RxConfig( rxConfig, datarate ); }
#define IN865_TX_CONFIG( )                        else if(region == LORAMAC_REGION_IN865) { return RegionIN865TxConfig( txConfig, txPower, txTimeOnAir ); }
#define IN865_LINK_ADR_REQ( )                     else if(region == LORAMAC_REGION_IN865) { return RegionIN865LinkAdrReq( linkAdrReq, drOut, txPowOut, nbRepOut, nbBytesParsed ); }
#define IN865_RX_PARAM_SETUP_REQ( )               else if(region == LORAMAC_REGION_IN865) { return RegionIN865RxParamSetupReq( rxParamSetupReq ); }
#define IN865_NEW_CHANNEL_REQ( )                  else if(region == LORAMAC_REGION_IN865) { return RegionIN865NewChannelReq( newChannelReq ); }
#define IN865_TX_PARAM_SETUP_REQ( )               else if(region == LORAMAC_REGION_IN865) { return RegionIN865TxParamSetupReq( txParamSetupReq ); }
#define IN865_DL_CHANNEL_REQ( )                   else if(region == LORAMAC_REGION_IN865) { return RegionIN865DlChannelReq( dlChannelReq ); }
#define IN865_ALTERNATE_DR( )                     else if(region == LORAMAC_REGION_IN865) { return RegionIN865AlternateDr( alternateDr ); }
#define IN865_CALC_BACKOFF( )                     else if(region == LORAMAC_REGION_IN865) { RegionIN865CalcBackOff( calcBackOff );}
#define IN865_NEXT_CHANNEL( )                     else if(region == LORAMAC_REGION_IN865) { return RegionIN865NextChannel( nextChanParams, channel, time, aggregatedTimeOff ); }
#define IN865_CHANNEL_ADD( )                      else if(region == LORAMAC_REGION_IN865) { return RegionIN865ChannelAdd( channelAdd ); }
#define IN865_CHANNEL_REMOVE( )                   else if(region == LORAMAC_REGION_IN865) { return RegionIN865ChannelsRemove( channelRemove ); }
#define IN865_CHANNEL_MANUAL_ADD( )               else if(region == LORAMAC_REGION_IN865) { return RegionIN865ChannelManualAdd( channelAdd ); }
#define IN865_CHANNEL_MANUAL_REMOVE( )            else if(region == LORAMAC_REGION_IN865) { return RegionIN865ChannelsRemove( channelRemove ); }
#define IN865_SET_CONTINUOUS_WAVE( )              else if(region == LORAMAC_REGION_IN865) { RegionIN865SetContinuousWave( continuousWave );}
#define IN865_APPLY_DR_OFFSET( )                  else if(region == LORAMAC_REGION_IN865) { return RegionIN865ApplyDrOffset( downlinkDwellTime, dr, drOffset ); }
#define IN865_GET_CHANNELS( )                     else if(region == LORAMAC_REGION_IN865) { return RegionIN865GetChannels( channels, size ); }
#define IN865_GET_CHANNEL_MASK( )                 else if(region == LORAMAC_REGION_IN865) { return RegionIN865GetChannelMask( channelmask, size ); }
#define IN865_FORCE_JOIN_DATARATE( )              else if(region == LORAMAC_REGION_IN865) { return RegionIN865ForceJoinDataRate( joinDr, alternateDr ); }
#else
#define IN865_IS_ACTIVE( )
#define IN865_GET_PHY_PARAM( )
#define IN865_SET_BAND_TX_DONE( )
#define IN865_INIT_DEFAULTS( )
#define IN865_VERIFY( )
#define IN865_APPLY_CF_LIST( )
#define IN865_CHAN_MASK_SET( )
#define IN865_ADR_NEXT( )
#define IN865_COMPUTE_RX_WINDOW_PARAMETERS( )
#define IN865_RX_CONFIG( )
#define IN865_TX_CONFIG( )
#define IN865_LINK_ADR_REQ( )
#define IN865_RX_PARAM_SETUP_REQ( )
#define IN865_NEW_CHANNEL_REQ( )
#define IN865_TX_PARAM_SETUP_REQ( )
#define IN865_DL_CHANNEL_REQ( )
#define IN865_ALTERNATE_DR( )
#define IN865_CALC_BACKOFF( )
#define IN865_NEXT_CHANNEL( )
#define IN865_CHANNEL_ADD( )
#define IN865_CHANNEL_REMOVE( )
#define IN865_CHANNEL_MANUAL_ADD( )
#define IN865_CHANNEL_MANUAL_REMOVE( )
#define IN865_SET_CONTINUOUS_WAVE( )
#define IN865_APPLY_DR_OFFSET( )
#define IN865_GET_CHANNELS( )
#define IN865_GET_CHANNEL_MASK( )
#define IN865_FORCE_JOIN_DATARATE( )
#endif

#ifdef REGION_US915
#include "RegionUS915.h"
#define US915_IS_ACTIVE( )                         else if(region == LORAMAC_REGION_US915) { return true; }
#define US915_GET_PHY_PARAM( )                     else if(region == LORAMAC_REGION_US915) { return RegionUS915GetPhyParam( getPhy ); }
#define US915_SET_BAND_TX_DONE( )                  else if(region == LORAMAC_REGION_US915) { RegionUS915SetBandTxDone( txDone );}
#define US915_INIT_DEFAULTS( )                     else if(region == LORAMAC_REGION_US915) { RegionUS915InitDefaults( type );}
#define US915_VERIFY( )                            else if(region == LORAMAC_REGION_US915) { return RegionUS915Verify( verify, phyAttribute ); }
#define US915_APPLY_CF_LIST( )                     else if(region == LORAMAC_REGION_US915) { RegionUS915ApplyCFList( applyCFList );}
#define US915_CHAN_MASK_SET( )                     else if(region == LORAMAC_REGION_US915) { return RegionUS915ChanMaskSet( chanMaskSet ); }
#define US915_ADR_NEXT( )                          else if(region == LORAMAC_REGION_US915) { return RegionUS915AdrNext( adrNext, drOut, txPowOut, adrAckCounter ); }
#define US915_COMPUTE_RX_WINDOW_PARAMETERS( )      else if(region == LORAMAC_REGION_US915) { RegionUS915ComputeRxWindowParameters( datarate, minRxSymbols, rxError, rxConfigParams );}
#define US915_RX_CONFIG( )                         else if(region == LORAMAC_REGION_US915) { return RegionUS915RxConfig( rxConfig, datarate ); }
#define US915_TX_CONFIG( )                         else if(region == LORAMAC_REGION_US915) { return RegionUS915TxConfig( txConfig, txPower, txTimeOnAir ); }
#define US915_LINK_ADR_REQ( )                      else if(region == LORAMAC_REGION_US915) { return RegionUS915LinkAdrReq( linkAdrReq, drOut, txPowOut, nbRepOut, nbBytesParsed ); }
#define US915_RX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_US915) { return RegionUS915RxParamSetupReq( rxParamSetupReq ); }
#define US915_NEW_CHANNEL_REQ( )                   else if(region == LORAMAC_REGION_US915) { return RegionUS915NewChannelReq( newChannelReq ); }
#define US915_TX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_US915) { return RegionUS915TxParamSetupReq( txParamSetupReq ); }
#define US915_DL_CHANNEL_REQ( )                    else if(region == LORAMAC_REGION_US915) { return RegionUS915DlChannelReq( dlChannelReq ); }
#define US915_ALTERNATE_DR( )                      else if(region == LORAMAC_REGION_US915) { return RegionUS915AlternateDr( alternateDr ); }
#define US915_CALC_BACKOFF( )                      else if(region == LORAMAC_REGION_US915) { RegionUS915CalcBackOff( calcBackOff );}
#define US915_NEXT_CHANNEL( )                      else if(region == LORAMAC_REGION_US915) { return RegionUS915NextChannel( nextChanParams, channel, time, aggregatedTimeOff ); }
#define US915_CHANNEL_ADD( )                       else if(region == LORAMAC_REGION_US915) { return RegionUS915ChannelAdd( channelAdd ); }
#define US915_CHANNEL_REMOVE( )                    else if(region == LORAMAC_REGION_US915) { return RegionUS915ChannelsRemove( channelRemove ); }
#define US915_CHANNEL_MANUAL_ADD( )                else if(region == LORAMAC_REGION_US915) { return RegionUS915ChannelManualAdd( channelAdd ); }
#define US915_CHANNEL_MANUAL_REMOVE( )             else if(region == LORAMAC_REGION_US915) { return RegionUS915ChannelsManualRemove( channelRemove ); }
#define US915_SET_CONTINUOUS_WAVE( )               else if(region == LORAMAC_REGION_US915) { RegionUS915SetContinuousWave( continuousWave );}
#define US915_APPLY_DR_OFFSET( )                   else if(region == LORAMAC_REGION_US915) { return RegionUS915ApplyDrOffset( downlinkDwellTime, dr, drOffset ); }
#define US915_GET_CHANNELS( )                      else if(region == LORAMAC_REGION_US915) { return RegionUS915GetChannels( channels, size ); }
#define US915_GET_CHANNEL_MASK( )                  else if(region == LORAMAC_REGION_US915) { return RegionUS915GetChannelMask( channelmask, size ); }
#define US915_GET_CHANNEL_MASK_REMAINING( )        else if(region == LORAMAC_REGION_US915) { return RegionUS915GetChannelMaskRemaining( channelmask, size ); }
#define US915_FORCE_JOIN_DATARATE( )               else if(region == LORAMAC_REGION_US915) { return RegionUS915ForceJoinDataRate( joinDr, alternateDr ); }
#else
#define US915_IS_ACTIVE( )
#define US915_GET_PHY_PARAM( )
#define US915_SET_BAND_TX_DONE( )
#define US915_INIT_DEFAULTS( )
#define US915_VERIFY( )
#define US915_APPLY_CF_LIST( )
#define US915_CHAN_MASK_SET( )
#define US915_ADR_NEXT( )
#define US915_COMPUTE_RX_WINDOW_PARAMETERS( )
#define US915_RX_CONFIG( )
#define US915_TX_CONFIG( )
#define US915_LINK_ADR_REQ( )
#define US915_RX_PARAM_SETUP_REQ( )
#define US915_NEW_CHANNEL_REQ( )
#define US915_TX_PARAM_SETUP_REQ( )
#define US915_DL_CHANNEL_REQ( )
#define US915_ALTERNATE_DR( )
#define US915_CALC_BACKOFF( )
#define US915_NEXT_CHANNEL( )
#define US915_CHANNEL_ADD( )
#define US915_CHANNEL_REMOVE( )
#define US915_CHANNEL_MANUAL_ADD( )
#define US915_CHANNEL_MANUAL_REMOVE( )
#define US915_SET_CONTINUOUS_WAVE( )
#define US915_APPLY_DR_OFFSET( )
#define US915_GET_CHANNELS( )
#define US915_GET_CHANNEL_MASK( )
#define US915_GET_CHANNEL_MASK_REMAINING( )
#define US915_FORCE_JOIN_DATARATE( )
#endif

#ifdef REGION_US915_HYBRID
#include "RegionUS915-Hybrid.h"
#define US915_HYBRID_IS_ACTIVE( )                         else if(region == LORAMAC_REGION_US915_HYBRID) { return true; }
#define US915_HYBRID_GET_PHY_PARAM( )                     else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridGetPhyParam( getPhy ); }
#define US915_HYBRID_SET_BAND_TX_DONE( )                  else if(region == LORAMAC_REGION_US915_HYBRID) { RegionUS915HybridSetBandTxDone( txDone );}
#define US915_HYBRID_INIT_DEFAULTS( )                     else if(region == LORAMAC_REGION_US915_HYBRID) { RegionUS915HybridInitDefaults( type );}
#define US915_HYBRID_VERIFY( )                            else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridVerify( verify, phyAttribute ); }
#define US915_HYBRID_APPLY_CF_LIST( )                     else if(region == LORAMAC_REGION_US915_HYBRID) { RegionUS915HybridApplyCFList( applyCFList );}
#define US915_HYBRID_CHAN_MASK_SET( )                     else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridChanMaskSet( chanMaskSet ); }
#define US915_HYBRID_ADR_NEXT( )                          else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridAdrNext( adrNext, drOut, txPowOut, adrAckCounter ); }
#define US915_HYBRID_COMPUTE_RX_WINDOW_PARAMETERS( )      else if(region == LORAMAC_REGION_US915_HYBRID) { RegionUS915HybridComputeRxWindowParameters( datarate, minRxSymbols, rxError, rxConfigParams );}
#define US915_HYBRID_RX_CONFIG( )                         else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridRxConfig( rxConfig, datarate ); }
#define US915_HYBRID_TX_CONFIG( )                         else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridTxConfig( txConfig, txPower, txTimeOnAir ); }
#define US915_HYBRID_LINK_ADR_REQ( )                      else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridLinkAdrReq( linkAdrReq, drOut, txPowOut, nbRepOut, nbBytesParsed ); }
#define US915_HYBRID_RX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridRxParamSetupReq( rxParamSetupReq ); }
#define US915_HYBRID_NEW_CHANNEL_REQ( )                   else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridNewChannelReq( newChannelReq ); }
#define US915_HYBRID_TX_PARAM_SETUP_REQ( )                else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridTxParamSetupReq( txParamSetupReq ); }
#define US915_HYBRID_DL_CHANNEL_REQ( )                    else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridDlChannelReq( dlChannelReq ); }
#define US915_HYBRID_ALTERNATE_DR( )                      else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridAlternateDr( alternateDr ); }
#define US915_HYBRID_CALC_BACKOFF( )                      else if(region == LORAMAC_REGION_US915_HYBRID) { RegionUS915HybridCalcBackOff( calcBackOff );}
#define US915_HYBRID_NEXT_CHANNEL( )                      else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridNextChannel( nextChanParams, channel, time, aggregatedTimeOff ); }
#define US915_HYBRID_CHANNEL_ADD( )                       else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridChannelAdd( channelAdd ); }
#define US915_HYBRID_CHANNEL_REMOVE( )                    else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridChannelsRemove( channelRemove ); }
#define US915_HYBRID_CHANNEL_MANUAL_ADD( )                else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridChannelManualAdd( channelAdd ); }
#define US915_HYBRID_CHANNEL_MANUAL_REMOVE( )             else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridChannelsManualRemove( channelRemove ); }
#define US915_HYBRID_SET_CONTINUOUS_WAVE( )               else if(region == LORAMAC_REGION_US915_HYBRID) { RegionUS915HybridSetContinuousWave( continuousWave );}
#define US915_HYBRID_APPLY_DR_OFFSET( )                   else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridApplyDrOffset( downlinkDwellTime, dr, drOffset ); }
#define US915_HYBRID_GET_CHANNELS( )                      else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridGetChannels( channels, size ); }
#define US915_HYBRID_GET_CHANNEL_MASK( )                  else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridGetChannelMask( channelmask, size ); }
#define US915_HYBRID_GET_CHANNEL_MASK_REMAINING( )        else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridGetChannelMaskRemaining( channelmask, size ); }
#define US915_HYBRID_FORCE_JOIN_DATARATE( )               else if(region == LORAMAC_REGION_US915_HYBRID) { return RegionUS915HybridForceJoinDataRate( joinDr, alternateDr ); }
#else
#define US915_HYBRID_IS_ACTIVE( )
#define US915_HYBRID_GET_PHY_PARAM( )
#define US915_HYBRID_SET_BAND_TX_DONE( )
#define US915_HYBRID_INIT_DEFAULTS( )
#define US915_HYBRID_VERIFY( )
#define US915_HYBRID_APPLY_CF_LIST( )
#define US915_HYBRID_CHAN_MASK_SET( )
#define US915_HYBRID_ADR_NEXT( )
#define US915_HYBRID_COMPUTE_RX_WINDOW_PARAMETERS( )
#define US915_HYBRID_RX_CONFIG( )
#define US915_HYBRID_TX_CONFIG( )
#define US915_HYBRID_LINK_ADR_REQ( )
#define US915_HYBRID_RX_PARAM_SETUP_REQ( )
#define US915_HYBRID_NEW_CHANNEL_REQ( )
#define US915_HYBRID_TX_PARAM_SETUP_REQ( )
#define US915_HYBRID_DL_CHANNEL_REQ( )
#define US915_HYBRID_ALTERNATE_DR( )
#define US915_HYBRID_CALC_BACKOFF( )
#define US915_HYBRID_NEXT_CHANNEL( )
#define US915_HYBRID_CHANNEL_ADD( )
#define US915_HYBRID_CHANNEL_REMOVE( )
#define US915_HYBRID_CHANNEL_MANUAL_ADD( )
#define US915_HYBRID_CHANNEL_MANUAL_REMOVE( )
#define US915_HYBRID_SET_CONTINUOUS_WAVE( )
#define US915_HYBRID_APPLY_DR_OFFSET( )
#define US915_HYBRID_GET_CHANNELS( )
#define US915_HYBRID_GET_CHANNEL_MASK( )
#define US915_HYBRID_GET_CHANNEL_MASK_REMAINING( )
#define US915_HYBRID_FORCE_JOIN_DATARATE( )
#endif

bool RegionIsActive( LoRaMacRegion_t region )
{
    if(region >= LORAMAC_REGION_MAX) {
        return false;
    }
    AS923_IS_ACTIVE( )
    AU915_IS_ACTIVE( )
    CN470_IS_ACTIVE( )
    CN779_IS_ACTIVE( )
    EU433_IS_ACTIVE( )
    EU868_IS_ACTIVE( )
    KR920_IS_ACTIVE( )
    IN865_IS_ACTIVE( )
    US915_IS_ACTIVE( )
    US915_HYBRID_IS_ACTIVE( )
    else {
        return false;
    }
}

IRAM_ATTR PhyParam_t RegionGetPhyParam( LoRaMacRegion_t region, GetPhyParams_t* getPhy )
{
    PhyParam_t phyParam = { 0 };

    if(region >= LORAMAC_REGION_MAX) {
        return phyParam;
    }
    AS923_GET_PHY_PARAM( )
    AU915_GET_PHY_PARAM( )
    CN470_GET_PHY_PARAM( )
    CN779_GET_PHY_PARAM( )
    EU433_GET_PHY_PARAM( )
    EU868_GET_PHY_PARAM( )
    KR920_GET_PHY_PARAM( )
    IN865_GET_PHY_PARAM( )
    US915_GET_PHY_PARAM( )
    US915_HYBRID_GET_PHY_PARAM( )
    else {
        return phyParam;
    }
}

IRAM_ATTR void RegionSetBandTxDone( LoRaMacRegion_t region, SetBandTxDoneParams_t* txDone )
{
    if(region >= LORAMAC_REGION_MAX) {
        return;
    }
    AS923_SET_BAND_TX_DONE( )
    AU915_SET_BAND_TX_DONE( )
    CN470_SET_BAND_TX_DONE( )
    CN779_SET_BAND_TX_DONE( )
    EU433_SET_BAND_TX_DONE( )
    EU868_SET_BAND_TX_DONE( )
    KR920_SET_BAND_TX_DONE( )
    IN865_SET_BAND_TX_DONE( )
    US915_SET_BAND_TX_DONE( )
    US915_HYBRID_SET_BAND_TX_DONE( )
}

void RegionInitDefaults( LoRaMacRegion_t region, InitType_t type )
{

    if(region >= LORAMAC_REGION_MAX) {
        return;
    }
    AS923_INIT_DEFAULTS( )
    AU915_INIT_DEFAULTS( )
    CN470_INIT_DEFAULTS( )
    CN779_INIT_DEFAULTS( )
    EU433_INIT_DEFAULTS( )
    EU868_INIT_DEFAULTS( )
    KR920_INIT_DEFAULTS( )
    IN865_INIT_DEFAULTS( )
    US915_INIT_DEFAULTS( )
    US915_HYBRID_INIT_DEFAULTS( )
}

bool RegionVerify( LoRaMacRegion_t region, VerifyParams_t* verify, PhyAttribute_t phyAttribute )
{

    if(region >= LORAMAC_REGION_MAX) {
        return false;
    }
    AS923_VERIFY( )
    AU915_VERIFY( )
    CN470_VERIFY( )
    CN779_VERIFY( )
    EU433_VERIFY( )
    EU868_VERIFY( )
    KR920_VERIFY( )
    IN865_VERIFY( )
    US915_VERIFY( )
    US915_HYBRID_VERIFY( )
    else {
        return false;
    }
}

void RegionApplyCFList( LoRaMacRegion_t region, ApplyCFListParams_t* applyCFList )
{

    if(region >= LORAMAC_REGION_MAX) {
        return;
    }
    AS923_APPLY_CF_LIST( )
    AU915_APPLY_CF_LIST( )
    CN470_APPLY_CF_LIST( )
    CN779_APPLY_CF_LIST( )
    EU433_APPLY_CF_LIST( )
    EU868_APPLY_CF_LIST( )
    KR920_APPLY_CF_LIST( )
    IN865_APPLY_CF_LIST( )
    US915_APPLY_CF_LIST( )
    US915_HYBRID_APPLY_CF_LIST( )
}

bool RegionChanMaskSet( LoRaMacRegion_t region, ChanMaskSetParams_t* chanMaskSet )
{

    if(region >= LORAMAC_REGION_MAX) {
        return false;
    }
    AS923_CHAN_MASK_SET( )
    AU915_CHAN_MASK_SET( )
    CN470_CHAN_MASK_SET( )
    CN779_CHAN_MASK_SET( )
    EU433_CHAN_MASK_SET( )
    EU868_CHAN_MASK_SET( )
    KR920_CHAN_MASK_SET( )
    IN865_CHAN_MASK_SET( )
    US915_CHAN_MASK_SET( )
    US915_HYBRID_CHAN_MASK_SET( )
    else {
        return false;
    }
}

bool RegionAdrNext( LoRaMacRegion_t region, AdrNextParams_t* adrNext, int8_t* drOut, int8_t* txPowOut, uint32_t* adrAckCounter )
{

    if(region >= LORAMAC_REGION_MAX) {
        return false;
    }
    AS923_ADR_NEXT( )
    AU915_ADR_NEXT( )
    CN470_ADR_NEXT( )
    CN779_ADR_NEXT( )
    EU433_ADR_NEXT( )
    EU868_ADR_NEXT( )
    KR920_ADR_NEXT( )
    IN865_ADR_NEXT( )
    US915_ADR_NEXT( )
    US915_HYBRID_ADR_NEXT( )
    else {
        return false;
    }
}

void RegionComputeRxWindowParameters( LoRaMacRegion_t region, int8_t datarate, uint8_t minRxSymbols, uint32_t rxError, RxConfigParams_t *rxConfigParams )
{

    if(region >= LORAMAC_REGION_MAX) {
        return;
    }
    AS923_COMPUTE_RX_WINDOW_PARAMETERS( )
    AU915_COMPUTE_RX_WINDOW_PARAMETERS( )
    CN470_COMPUTE_RX_WINDOW_PARAMETERS( )
    CN779_COMPUTE_RX_WINDOW_PARAMETERS( )
    EU433_COMPUTE_RX_WINDOW_PARAMETERS( )
    EU868_COMPUTE_RX_WINDOW_PARAMETERS( )
    KR920_COMPUTE_RX_WINDOW_PARAMETERS( )
    IN865_COMPUTE_RX_WINDOW_PARAMETERS( )
    US915_COMPUTE_RX_WINDOW_PARAMETERS( )
    US915_HYBRID_COMPUTE_RX_WINDOW_PARAMETERS( )
}

bool RegionRxConfig( LoRaMacRegion_t region, RxConfigParams_t* rxConfig, int8_t* datarate )
{

    if(region >= LORAMAC_REGION_MAX) {
        return false;
    }
    AS923_RX_CONFIG( )
    AU915_RX_CONFIG( )
    CN470_RX_CONFIG( )
    CN779_RX_CONFIG( )
    EU433_RX_CONFIG( )
    EU868_RX_CONFIG( )
    KR920_RX_CONFIG( )
    IN865_RX_CONFIG( )
    US915_RX_CONFIG( )
    US915_HYBRID_RX_CONFIG( )
    else {
        return false;
    }
}

bool RegionTxConfig( LoRaMacRegion_t region, TxConfigParams_t* txConfig, int8_t* txPower, TimerTime_t* txTimeOnAir )
{

    if(region >= LORAMAC_REGION_MAX) {
        return false;
    }
    AS923_TX_CONFIG( )
    AU915_TX_CONFIG( )
    CN470_TX_CONFIG( )
    CN779_TX_CONFIG( )
    EU433_TX_CONFIG( )
    EU868_TX_CONFIG( )
    KR920_TX_CONFIG( )
    IN865_TX_CONFIG( )
    US915_TX_CONFIG( )
    US915_HYBRID_TX_CONFIG( )
    else {
        return false;
    }
}

uint8_t RegionLinkAdrReq( LoRaMacRegion_t region, LinkAdrReqParams_t* linkAdrReq, int8_t* drOut, int8_t* txPowOut, uint8_t* nbRepOut, uint8_t* nbBytesParsed )
{

    if(region >= LORAMAC_REGION_MAX) {
        return 0;
    }
    AS923_LINK_ADR_REQ( )
    AU915_LINK_ADR_REQ( )
    CN470_LINK_ADR_REQ( )
    CN779_LINK_ADR_REQ( )
    EU433_LINK_ADR_REQ( )
    EU868_LINK_ADR_REQ( )
    KR920_LINK_ADR_REQ( )
    IN865_LINK_ADR_REQ( )
    US915_LINK_ADR_REQ( )
    US915_HYBRID_LINK_ADR_REQ( )
    else {
        return 0;
    }
}

uint8_t RegionRxParamSetupReq( LoRaMacRegion_t region, RxParamSetupReqParams_t* rxParamSetupReq )
{

    if(region >= LORAMAC_REGION_MAX) {
        return 0;
    }
    AS923_RX_PARAM_SETUP_REQ( )
    AU915_RX_PARAM_SETUP_REQ( )
    CN470_RX_PARAM_SETUP_REQ( )
    CN779_RX_PARAM_SETUP_REQ( )
    EU433_RX_PARAM_SETUP_REQ( )
    EU868_RX_PARAM_SETUP_REQ( )
    KR920_RX_PARAM_SETUP_REQ( )
    IN865_RX_PARAM_SETUP_REQ( )
    US915_RX_PARAM_SETUP_REQ( )
    US915_HYBRID_RX_PARAM_SETUP_REQ( )
    else {
        return 0;
    }
}

uint8_t RegionNewChannelReq( LoRaMacRegion_t region, NewChannelReqParams_t* newChannelReq )
{

    if(region >= LORAMAC_REGION_MAX) {
        return 0;
    }
    AS923_NEW_CHANNEL_REQ( )
    AU915_NEW_CHANNEL_REQ( )
    CN470_NEW_CHANNEL_REQ( )
    CN779_NEW_CHANNEL_REQ( )
    EU433_NEW_CHANNEL_REQ( )
    EU868_NEW_CHANNEL_REQ( )
    KR920_NEW_CHANNEL_REQ( )
    IN865_NEW_CHANNEL_REQ( )
    US915_NEW_CHANNEL_REQ( )
    US915_HYBRID_NEW_CHANNEL_REQ( )
    else {
        return 0;
    }
}

int8_t RegionTxParamSetupReq( LoRaMacRegion_t region, TxParamSetupReqParams_t* txParamSetupReq )
{

    if(region >= LORAMAC_REGION_MAX) {
        return 0;
    }
    AS923_TX_PARAM_SETUP_REQ( )
    AU915_TX_PARAM_SETUP_REQ( )
    CN470_TX_PARAM_SETUP_REQ( )
    CN779_TX_PARAM_SETUP_REQ( )
    EU433_TX_PARAM_SETUP_REQ( )
    EU868_TX_PARAM_SETUP_REQ( )
    KR920_TX_PARAM_SETUP_REQ( )
    IN865_TX_PARAM_SETUP_REQ( )
    US915_TX_PARAM_SETUP_REQ( )
    US915_HYBRID_TX_PARAM_SETUP_REQ( )
    else {
        return 0;
    }
}

uint8_t RegionDlChannelReq( LoRaMacRegion_t region, DlChannelReqParams_t* dlChannelReq )
{

    if(region >= LORAMAC_REGION_MAX) {
        return 0;
    }
    AS923_DL_CHANNEL_REQ( )
    AU915_DL_CHANNEL_REQ( )
    CN470_DL_CHANNEL_REQ( )
    CN779_DL_CHANNEL_REQ( )
    EU433_DL_CHANNEL_REQ( )
    EU868_DL_CHANNEL_REQ( )
    KR920_DL_CHANNEL_REQ( )
    IN865_DL_CHANNEL_REQ( )
    US915_DL_CHANNEL_REQ( )
    US915_HYBRID_DL_CHANNEL_REQ( )
    else {
        return 0;
    }
}

int8_t RegionAlternateDr( LoRaMacRegion_t region, AlternateDrParams_t* alternateDr )
{

    if(region >= LORAMAC_REGION_MAX) {
        return 0;
    }
    AS923_ALTERNATE_DR( )
    AU915_ALTERNATE_DR( )
    CN470_ALTERNATE_DR( )
    CN779_ALTERNATE_DR( )
    EU433_ALTERNATE_DR( )
    EU868_ALTERNATE_DR( )
    KR920_ALTERNATE_DR( )
    IN865_ALTERNATE_DR( )
    US915_ALTERNATE_DR( )
    US915_HYBRID_ALTERNATE_DR( )
    else {
        return 0;
    }
}

void RegionCalcBackOff( LoRaMacRegion_t region, CalcBackOffParams_t* calcBackOff )
{

    if(region >= LORAMAC_REGION_MAX) {
        return;
    }
    AS923_CALC_BACKOFF( )
    AU915_CALC_BACKOFF( )
    CN470_CALC_BACKOFF( )
    CN779_CALC_BACKOFF( )
    EU433_CALC_BACKOFF( )
    EU868_CALC_BACKOFF( )
    KR920_CALC_BACKOFF( )
    IN865_CALC_BACKOFF( )
    US915_CALC_BACKOFF( )
    US915_HYBRID_CALC_BACKOFF( )
}

bool RegionNextChannel( LoRaMacRegion_t region, NextChanParams_t* nextChanParams, uint8_t* channel, TimerTime_t* time, TimerTime_t* aggregatedTimeOff )
{

    if(region >= LORAMAC_REGION_MAX) {
        return false;
    }
    AS923_NEXT_CHANNEL( )
    AU915_NEXT_CHANNEL( )
    CN470_NEXT_CHANNEL( )
    CN779_NEXT_CHANNEL( )
    EU433_NEXT_CHANNEL( )
    EU868_NEXT_CHANNEL( )
    KR920_NEXT_CHANNEL( )
    IN865_NEXT_CHANNEL( )
    US915_NEXT_CHANNEL( )
    US915_HYBRID_NEXT_CHANNEL( )
    else {
        return false;
    }
}

LoRaMacStatus_t RegionChannelAdd( LoRaMacRegion_t region, ChannelAddParams_t* channelAdd )
{

    if(region >= LORAMAC_REGION_MAX) {
        return LORAMAC_STATUS_PARAMETER_INVALID;
    }
    AS923_CHANNEL_ADD( )
    AU915_CHANNEL_ADD( )
    CN470_CHANNEL_ADD( )
    CN779_CHANNEL_ADD( )
    EU433_CHANNEL_ADD( )
    EU868_CHANNEL_ADD( )
    KR920_CHANNEL_ADD( )
    IN865_CHANNEL_ADD( )
    US915_CHANNEL_ADD( )
    US915_HYBRID_CHANNEL_ADD( )
    else {
        return LORAMAC_STATUS_PARAMETER_INVALID;
    }
}

LoRaMacStatus_t RegionChannelManualAdd( LoRaMacRegion_t region, ChannelAddParams_t* channelAdd )
{

    if(region >= LORAMAC_REGION_MAX) {
        return LORAMAC_STATUS_PARAMETER_INVALID;
    }
    AS923_CHANNEL_MANUAL_ADD( )
    AU915_CHANNEL_MANUAL_ADD( )
    EU868_CHANNEL_MANUAL_ADD( )
    US915_CHANNEL_MANUAL_ADD( )
    CN470_CHANNEL_MANUAL_ADD( )
    IN865_CHANNEL_MANUAL_ADD( )
    US915_HYBRID_CHANNEL_MANUAL_ADD( )
    else {
        return LORAMAC_STATUS_PARAMETER_INVALID;
    }
}

bool RegionChannelsRemove( LoRaMacRegion_t region, ChannelRemoveParams_t* channelRemove )
{

    if(region >= LORAMAC_REGION_MAX) {
        return false;
    }
    AS923_CHANNEL_REMOVE( )
    AU915_CHANNEL_REMOVE( )
    CN470_CHANNEL_REMOVE( )
    CN779_CHANNEL_REMOVE( )
    EU433_CHANNEL_REMOVE( )
    EU868_CHANNEL_REMOVE( )
    KR920_CHANNEL_REMOVE( )
    IN865_CHANNEL_REMOVE( )
    US915_CHANNEL_REMOVE( )
    US915_HYBRID_CHANNEL_REMOVE( )
    else {
        return false;
    }
}

bool RegionChannelsManualRemove( LoRaMacRegion_t region, ChannelRemoveParams_t* channelRemove )
{

    if(region >= LORAMAC_REGION_MAX) {
        return false;
    }
    AS923_CHANNEL_MANUAL_REMOVE( )
    AU915_CHANNEL_MANUAL_REMOVE( )
    EU868_CHANNEL_MANUAL_REMOVE( )
    US915_CHANNEL_MANUAL_REMOVE( )
    CN470_CHANNEL_MANUAL_REMOVE( )
    IN865_CHANNEL_MANUAL_REMOVE( )
    US915_HYBRID_CHANNEL_MANUAL_REMOVE( )
    US915_HYBRID_CHANNEL_REMOVE( )
    else {
        return false;
    }
}

void RegionSetContinuousWave( LoRaMacRegion_t region, ContinuousWaveParams_t* continuousWave )
{

    if(region >= LORAMAC_REGION_MAX) {
        return;
    }
    AS923_SET_CONTINUOUS_WAVE( )
    AU915_SET_CONTINUOUS_WAVE( )
    CN470_SET_CONTINUOUS_WAVE( )
    CN779_SET_CONTINUOUS_WAVE( )
    EU433_SET_CONTINUOUS_WAVE( )
    EU868_SET_CONTINUOUS_WAVE( )
    KR920_SET_CONTINUOUS_WAVE( )
    IN865_SET_CONTINUOUS_WAVE( )
    US915_SET_CONTINUOUS_WAVE( )
    US915_HYBRID_SET_CONTINUOUS_WAVE( )
}

uint8_t RegionApplyDrOffset( LoRaMacRegion_t region, uint8_t downlinkDwellTime, int8_t dr, int8_t drOffset )
{

    if(region >= LORAMAC_REGION_MAX) {
        return dr;
    }
    AS923_APPLY_DR_OFFSET( )
    AU915_APPLY_DR_OFFSET( )
    CN470_APPLY_DR_OFFSET( )
    CN779_APPLY_DR_OFFSET( )
    EU433_APPLY_DR_OFFSET( )
    EU868_APPLY_DR_OFFSET( )
    KR920_APPLY_DR_OFFSET( )
    IN865_APPLY_DR_OFFSET( )
    US915_APPLY_DR_OFFSET( )
    US915_HYBRID_APPLY_DR_OFFSET( )
    US915_HYBRID_CHANNEL_REMOVE( )
    else {
        return dr;
    }
}

bool RegionGetChannels( LoRaMacRegion_t region, ChannelParams_t** channels, uint32_t *size )
{

    if(region >= LORAMAC_REGION_MAX) {
        return false;
    }
    AS923_GET_CHANNELS( )
    AU915_GET_CHANNELS( )
    EU868_GET_CHANNELS( )
    IN865_GET_CHANNELS( )
    US915_GET_CHANNELS( )
    US915_HYBRID_GET_CHANNELS( )
    US915_HYBRID_CHANNEL_REMOVE( )
    else {
        return false;
    }
}

bool RegionGetChannelMask(LoRaMacRegion_t region, uint16_t **channelmask, uint32_t *size ) {

    if(region >= LORAMAC_REGION_MAX) {
        return false;
    }
    AS923_GET_CHANNEL_MASK( )
    AU915_GET_CHANNEL_MASK( )
    EU868_GET_CHANNEL_MASK( )
    IN865_GET_CHANNEL_MASK( )
    US915_GET_CHANNEL_MASK( )
    US915_HYBRID_GET_CHANNEL_MASK( )
    US915_HYBRID_CHANNEL_REMOVE( )
    else {
        return false;
    }
}

bool RegionGetChannelMaskRemaining(LoRaMacRegion_t region, uint16_t **channelmask, uint32_t *size ) {

    if(region >= LORAMAC_REGION_MAX) {
        return false;
    }
    AU915_GET_CHANNEL_MASK_REMAINING( )
    US915_GET_CHANNEL_MASK_REMAINING( )
    US915_HYBRID_GET_CHANNEL_MASK_REMAINING( )
    US915_HYBRID_CHANNEL_REMOVE( )
    else {
        return false;
    }
}

bool RegionForceJoinDataRate( LoRaMacRegion_t region, int8_t joinDr, AlternateDrParams_t* alternateDr )
{

    if(region >= LORAMAC_REGION_MAX) {
        return false;
    }
    AS923_FORCE_JOIN_DATARATE( )
    AU915_FORCE_JOIN_DATARATE( )
    EU868_FORCE_JOIN_DATARATE( )
    US915_FORCE_JOIN_DATARATE( )
    CN470_FORCE_JOIN_DATARATE( )
    IN865_FORCE_JOIN_DATARATE( )
    US915_HYBRID_FORCE_JOIN_DATARATE( )
    US915_HYBRID_CHANNEL_REMOVE( )
    else {
        return false;
    }
}
