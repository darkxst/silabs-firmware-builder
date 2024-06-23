/**
 * @file cmd_handlers.c
 * @copyright 2022 Silicon Laboratories Inc.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <AppTimer.h>
#include <SyncEvent.h>
#include <ZAF_Common_interface.h>
#include <ZW_application_transport_interface.h>
#include <Min2Max2.h>
#include <SizeOf.h>
#include <zpal_misc.h>
#include <zpal_watchdog.h>
//#define DEBUGPRINT
#include <DebugPrint.h>

#include "app_node_info.h"
#include "virtual_slave_node_info.h"
#include "cmd_handlers.h"
#include "cmds_management.h"
#include "cmds_security.h"
#include "cmds_rf.h"
#include "SerialAPI.h"
#include "app.h"
#include "serialapi_file.h"
#include "utils.h"
#include "nvm_backup_restore.h"
#include "zaf_protocol_config.h"

#if SUPPORT_ZW_AES_ECB
#include <ZW_aes_api.h>
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

extern bool bTxStatusReportEnabled;

SSyncEventArg1 LearnModeStatusCb = {.uFunctor.pFunction = 0}; // Ensure function pointer is initialized
SSyncEvent SetDefaultCB = {.uFunctor.pFunction = 0};         // Ensure function pointer is initialized

#ifdef ZW_CONTROLLER
static uint8_t addState = 0;
uint8_t funcID_ComplHandler_ZW_NodeManagement;
uint8_t nodeManagement_Func_ID;
#endif


#ifdef ZW_CONTROLLER
static void SetupNodeManagement(const comm_interface_frame_ptr frame, uint8_t funcID_offet)
{
  nodeManagement_Func_ID = frame->cmd;
  funcID_ComplHandler_ZW_NodeManagement = *(frame->payload + funcID_offet);
  set_state_and_notify(stateIdle);
  addState = 0;
}
#endif


#if SUPPORT_ZW_INITIATE_SHUTDOWN
/*
   This callback function called from protocol just before going into deep sleep (Deep Sleep)
   The function itself sends a respond to the host notifying it that the device is ready to go into deep sleep.
*/
static void Initiate_shutdown_cb(void)
{
  // 0x1 0x03 0x00 0xd9
  const uint8_t status = 0x01;
  comm_interface_transmit_frame(FUNC_ID_ZW_INITIATE_SHUTDOWN, RESPONSE, &status, sizeof(status), NULL);
  comm_interface_wait_transmit_done();
}

/*
  HOST->ZW
  ZW-HOST 0x01
*/
ZW_ADD_CMD(FUNC_ID_ZW_INITIATE_SHUTDOWN)
{
  AppTimerStopAll();
  if (InitiateShutdown(&Initiate_shutdown_cb))
  {
    set_state_and_notify(stateIdle);
  }
  else
  {
    // somthing went wrong we failed to start the graceful shutdown
    DoRespond(0);
  }
}

#endif


#if SUPPORT_FUNC_ID_CLEAR_TX_TIMERS
static void ClearTxTimers(void)
{
  SZwaveCommandPackage CommandPackage = { .eCommandType = EZWAVECOMMANDTYPE_CLEAR_TX_TIMERS };

  // Put the package on queue (and DO wait for it, since there is no feedback to serial master)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&CommandPackage, 500);

  if (EQUEUENOTIFYING_STATUS_SUCCESS != QueueStatus)
  {
    DPRINT("Warning: Failed to clear Tx timers");
  }
}

ZW_ADD_CMD(FUNC_ID_CLEAR_TX_TIMERS)
{
  /* HOST->ZW: */
  ClearTxTimers();
  set_state(stateIdle);
}
#endif /* SUPPORT_FUNC_ID_CLEAR_TX_TIMERS */


#if SUPPORT_FUNC_ID_GET_TX_TIMERS
ZW_ADD_CMD(FUNC_ID_GET_TX_TIMERS)
{
  /* HOST->ZW: */
  /* ZW->HOST: channel0TxTimer_MSB | channel0TxTimer_Next_MSB | channel0TxTimer_Next_LSB | channel0TxTimerLSB | */
  /*           channel1TxTimer_MSB | channel1TxTimer_Next_MSB | channel1TxTimer_Next_LSB | channel1TxTimerLSB | */
  /*           channel2TxTimer_MSB | channel2TxTimer_Next_MSB | channel2TxTimer_Next_LSB | channel2TxTimerLSB | */
  /*           channel3TxTimer_MSB | channel3TxTimer_Next_MSB | channel3TxTimer_Next_LSB | channel3TxTimerLSB | */
  /*           channel4TxTimer_MSB | channel4TxTimer_Next_MSB | channel4TxTimer_Next_LSB | channel4TxTimerLSB   */

  // Copy current Network Statistic counters
  const zpal_radio_network_stats_t *sRFStats = ZAF_getNetworkStatistics();

  uint8_t i = 0;
  // Copy with endian swap
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[0] >> 24) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[0] >> 16) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[0] >> 8)  & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[0] >> 0)  & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[1] >> 24) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[1] >> 16) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[1] >> 8)  & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[1] >> 0)  & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[2] >> 24) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[2] >> 16) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[2] >> 8)  & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[2] >> 0)  & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[3] >> 24) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[3] >> 16) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[3] >> 8)  & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[3] >> 0)  & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[4] >> 24) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[4] >> 16) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[4] >> 8)  & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_time_channel[4] >> 0)  & 0xff);

  DoRespond_workbuf(i);
}
#endif /* SUPPORT_FUNC_ID_GET_TX_TIMERS */


#if SUPPORT_ZW_GET_BACKGROUND_RSSI
static void GetBackgroundRSSI(RSSI_LEVELS *noise_levels)
{
  SZwaveCommandPackage cmdPackage = {.eCommandType = EZWAVECOMMANDTYPE_GET_BACKGROUND_RSSI};
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&cmdPackage, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
  SZwaveCommandStatusPackage cmdStatus = { 0 };
  if (GetCommandResponse(&cmdStatus, EZWAVECOMMANDSTATUS_GET_BACKGROUND_RSSI))
  {
    memcpy((uint8_t *)noise_levels, cmdStatus.Content.GetBackgroundRssiStatus.rssi, sizeof(RSSI_LEVELS));
    return;
  }
  assert(false); // FIXME We should have more intelligent error handling, we shouldnt assert here.
}

ZW_ADD_CMD(FUNC_ID_ZW_GET_BACKGROUND_RSSI)
{
  /* HOST->ZW: (no arguments) */
  /* ZW->HOST: RES | RSSI Ch0 | RSSI Ch1 | RSSI Ch2  */
  GetBackgroundRSSI((RSSI_LEVELS*)&compl_workbuf[0]);
  DoRespond_workbuf(sizeof(RSSI_LEVELS));
}
#endif


#if SUPPORT_ZW_CLEAR_NETWORK_STATS
static void ClearNetworkStats(void)
{
  SZwaveCommandPackage CommandPackage = {.eCommandType = EZWAVECOMMANDTYPE_CLEAR_NETWORK_STATISTICS};
  // Put the package on queue (and dont wait for it)
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&CommandPackage, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
}

ZW_ADD_CMD(FUNC_ID_ZW_CLEAR_NETWORK_STATS)
{
  /* HOST->ZW: */
  /* ZW->HOST: 0x01 */
  ClearNetworkStats();
  DoRespond(1);
}
#endif /* SUPPORT_ZW_CLEAR_NETWORK_STATS */


#if SUPPORT_ZW_GET_NETWORK_STATS
ZW_ADD_CMD(FUNC_ID_ZW_GET_NETWORK_STATS)
{
  /* HOST->ZW: */
  /* ZW->HOST: wRFTxFrames_MSB | wRFTxFrames_LSB | */
  /*           wRFTxLBTBackOffs_MSB | wRFTxLBTBackOffs_LSB | */
  /*           wRFRxFrames_MSB | wRFRxFrames_LSB */
  /*           wRFRxLRCErrors_MSB | wRFRxLRCErrors_LSB */
  /*           wRFRxCRCErrors_MSB | wRFRxCRCErrors_LSB */
  /*           wRFRxForeignHomeID_MSB | wRFRxForeignHomeID_LSB */

  // Copy current Network Statistic counters
  const zpal_radio_network_stats_t *sRFStats = ZAF_getNetworkStatistics();

  int i = 0;
  // Copy with endian swap
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_frames >> 8) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_frames >> 0) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_lbt_back_offs >> 8) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->tx_lbt_back_offs >> 0) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->rx_frames >> 8) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->rx_frames >> 0) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->rx_lrc_errors >> 8) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->rx_lrc_errors >> 0) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->rx_crc_errors >> 8) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->rx_crc_errors >> 0) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->rx_foreign_home_id >> 8) & 0xff);
  compl_workbuf[i++] = (uint8_t)((sRFStats->rx_foreign_home_id >> 0) & 0xff);

  DoRespond_workbuf((uint8_t)i);
}
#endif /* SUPPORT_ZW_GET_NETWORK_STATS */


#if SUPPORT_ZW_SET_RF_RECEIVE_MODE
uint8_t SetRFReceiveMode(uint8_t mode)
{
  SZwaveCommandPackage pCmdPackage = {
      .eCommandType = EZWAVECOMMANDTYPE_SET_RF_RECEIVE_MODE,
      .uCommandParams.SetRfReceiveMode.mode = mode};

  // Put the Command on queue (and dont wait for it, queue must be empty)
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&pCmdPackage, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
  SZwaveCommandStatusPackage cmdStatus = { 0 };
  if (GetCommandResponse(&cmdStatus, EZWAVECOMMANDSTATUS_SET_RF_RECEIVE_MODE))
  {
    return cmdStatus.Content.SetRFReceiveModeStatus.result;
  }
  assert(0);
  return 0;
}

ZW_ADD_CMD(FUNC_ID_ZW_SET_RF_RECEIVE_MODE)
{
  /* HOST->ZW: mode */
  /* ZW->HOST: retVal */
  const uint8_t retVal = SetRFReceiveMode(frame->payload[0]);
  DoRespond(retVal);
}
#endif /* SUPPORT_ZW_SET_RF_RECEIVE_MODE */


#if SUPPORT_ZW_SEND_NODE_INFORMATION
uint8_t funcID_ComplHandler_ZW_SendNodeInformation;

static uint8_t SendNodeInformation(uint16_t destID, uint8_t txOptions, ZW_TX_Callback_t pCallBack)
{
  SZwaveTransmitPackage FramePackage = {
    .uTransmitParams.NodeInfo.DestNodeId = destID,
    .uTransmitParams.NodeInfo.TransmitOptions = txOptions,
    .uTransmitParams.NodeInfo.Handle = (ZW_Void_Callback_t)pCallBack,
    .eTransmitType = EZWAVETRANSMITTYPE_NODEINFORMATION
  };
  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus;
  QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

/*=====================   ComplHandler_ZW_SendNodeInformation   =============
**    Completion handler for ZW_SendNodeInformation
**
**--------------------------------------------------------------------------*/
static void /* RET  Nothing                     */
ZCB_ComplHandler_ZW_SendNodeInformation(
    uint8_t txStatus, /* IN   Transmit completion status  */
    __attribute__((unused)) TX_STATUS_TYPE *txStatusReport)
{
  BYTE_IN_AR(compl_workbuf, 0) = funcID_ComplHandler_ZW_SendNodeInformation;
  BYTE_IN_AR(compl_workbuf, 1) = txStatus;
  Request(FUNC_ID_ZW_SEND_NODE_INFORMATION, compl_workbuf, 2);
}

ZW_ADD_CMD(FUNC_ID_ZW_SEND_NODE_INFORMATION)
{
  /* HOST->ZW: destNode | txOptions | funcID */
  /* ZW->HOST: retVal */
  uint8_t offset = 0;
  node_id_t destNode = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  funcID_ComplHandler_ZW_SendNodeInformation = frame->payload[offset + 2];
  const uint8_t retVal = SendNodeInformation(destNode, frame->payload[offset + 1], (frame->payload[offset + 2] != 0) ? &ZCB_ComplHandler_ZW_SendNodeInformation : NULL);
  DoRespond(retVal);
}
#endif /* SUPPORT_ZW_SEND_NODE_INFORMATION */


#if SUPPORT_ZW_SECURITY_SETUP
ZW_ADD_CMD(FUNC_ID_ZW_SECURITY_SETUP)
{
  uint8_t length;
  func_id_zw_security_setup(frame_payload_len(frame), frame->payload, compl_workbuf, &length);
  DoRespond_workbuf(length);
}
#endif


#if SUPPORT_ZW_SEND_DATA || SUPPORT_ZW_SEND_DATA_BRIDGE
uint8_t funcID_ComplHandler_ZW_SendData;
#endif

#if SUPPORT_ZW_SEND_DATA || SUPPORT_ZW_SEND_DATA_EX || SUPPORT_ZW_SEND_DATA_BRIDGE
static void
GenerateTxStatusRequest(
    uint8_t cmd,
    uint8_t txStatusfuncID,
    uint8_t txStatus,
    TX_STATUS_TYPE *txStatusReport) /* IN   Transmit completion status  */
{
  uint8_t bIdx = 0;
  BYTE_IN_AR(compl_workbuf, bIdx++) = txStatusfuncID;
  BYTE_IN_AR(compl_workbuf, bIdx++) = txStatus;
  if (bTxStatusReportEnabled /* Do HOST want txStatusReport */
      && txStatusReport)     /* Check if detailed info is available from protocol */
  {
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)((((txStatusReport->TransmitTicks / 10) & 0xFFFFFF) >> 8) & 0xFF);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(((txStatusReport->TransmitTicks / 10) & 0xFFFFFF) & 0xFF);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->bRepeaters);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->rssi_values.incoming[0]);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->rssi_values.incoming[1]);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->rssi_values.incoming[2]);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->rssi_values.incoming[3]);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->rssi_values.incoming[4]);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->bACKChannelNo);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->bLastTxChannelNo);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->bRouteSchemeState);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->pLastUsedRoute[0]);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->pLastUsedRoute[1]);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->pLastUsedRoute[2]);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->pLastUsedRoute[3]);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->pLastUsedRoute[4]);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->bRouteTries);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->bLastFailedLink.from);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->bLastFailedLink.to);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->bUsedTxpower);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->bMeasuredNoiseFloor);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->bDestinationAckUsedTxPower);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->bDestinationAckMeasuredRSSI);
    BYTE_IN_AR(compl_workbuf, bIdx++) = (uint8_t)(txStatusReport->bDestinationAckMeasuredNoiseFloor);
  }
  Request(cmd, compl_workbuf, bIdx);
}
#endif

#if SUPPORT_ZW_SEND_DATA
/*======================   ComplHandler_ZW_SendData   ========================
**    Completion handler for ZW_SendData
**
**--------------------------------------------------------------------------*/
static void /* RET  Nothing                     */
ZCB_ComplHandler_ZW_SendData(
    uint8_t txStatus,
    TX_STATUS_TYPE *txStatusReport) /* IN   Transmit completion status  */
{
  GenerateTxStatusRequest(FUNC_ID_ZW_SEND_DATA, funcID_ComplHandler_ZW_SendData, txStatus, txStatusReport);
}

static uint8_t SendData(uint16_t nodeID, const uint8_t *pData, uint8_t dataLength, uint8_t txOptions, ZW_TX_Callback_t pCallBack)
{
  SZwaveTransmitPackage FramePackage = { 
    .uTransmitParams.SendData.DestNodeId = nodeID,
    .uTransmitParams.SendData.FrameConfig.TransmitOptions = txOptions,
    .uTransmitParams.SendData.FrameConfig.Handle = pCallBack,
    .eTransmitType = EZWAVETRANSMITTYPE_STD,
    .uTransmitParams.SendData.FrameConfig.iFrameLength = dataLength,
   };

  memcpy(FramePackage.uTransmitParams.SendData.FrameConfig.aFrame, pData, dataLength);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0));
}

ZW_ADD_CMD(FUNC_ID_ZW_SEND_DATA)
{
  /* HOST->ZW: nodeID | dataLength | pData[] | txOptions | funcID */
  /* ZW->HOST: RetVal */
  /* If RetVal == false -> no callback */
  /* If RetVal == true then callback returns with */
  /* ZW->HOST: funcID | txStatus | wTransmitTicksMSB | wTransmitTicksLSB | bRepeaters | rssi_values.incoming[0] |
    *           rssi_values.incoming[1] | rssi_values.incoming[2] | rssi_values.incoming[3] | rssi_values.incoming[4] |
    *           bRouteSchemeState | repeater0 | repeater1 | repeater2 | repeater3 | routespeed |
    *           bRouteTries | bLastFailedLink.from | bLastFailedLink.to |
    *           bUsedTxpower | bMeasuredNoiseFloor | bAckDestinationUsedTxPower | bDestinationAckMeasuredRSSI |
    *           bDestinationckMeasuredNoiseFloor */
  uint8_t  offset = 0;
  node_id_t nodeId = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  uint8_t dataLength = frame->payload[offset + 1];

  assert(dataLength <= BUF_SIZE_RX);
  dataLength = MIN(dataLength, BUF_SIZE_RX);
  const uint8_t * const pSerInData = frame->payload + offset + 2;
  funcID_ComplHandler_ZW_SendData = frame->payload[offset + 3 + dataLength];

  // Create transmit frame package
  const uint8_t retVal = SendData(nodeId, pSerInData, dataLength, frame->payload[offset + 2 + dataLength],
                    (funcID_ComplHandler_ZW_SendData) ? &ZCB_ComplHandler_ZW_SendData : NULL);
  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_SEND_DATA_EX
uint8_t funcID_ComplHandler_ZW_SendDataEx;

/*======================   ComplHandler_ZW_SendDataEx   ========================
**    Completion handler for ZW_SendDataEx
**
**--------------------------------------------------------------------------*/
static void /* RET  Nothing                     */
ZCB_ComplHandler_ZW_SendDataEx(
    uint8_t txStatus,
    TX_STATUS_TYPE *txStatusReport) /* IN   Transmit completion status  */
{
  GenerateTxStatusRequest(FUNC_ID_ZW_SEND_DATA_EX, funcID_ComplHandler_ZW_SendDataEx, txStatus, txStatusReport);
}

static uint8_t SendDataEx(uint16_t nodeID, uint8_t *pData, uint8_t dataLength,
                          uint8_t txOptions, uint8_t txSecOptions, uint8_t txOptions2, uint8_t secKeyType,
                          ZW_TX_Callback_t pCallBack)
{
  // Create transmit frame package
  SZwaveTransmitPackage FramePackage = {
    .uTransmitParams.SendDataEx.DestNodeId = nodeID,
    .uTransmitParams.SendDataEx.TransmitSecurityOptions = txSecOptions,
    .uTransmitParams.SendDataEx.TransmitOptions2 = txOptions2,
    .uTransmitParams.SendDataEx.eKeyType = secKeyType,
    .uTransmitParams.SendDataEx.FrameConfig.TransmitOptions = txOptions,
    .uTransmitParams.SendDataEx.FrameConfig.Handle = pCallBack,
    .uTransmitParams.SendDataEx.FrameConfig.iFrameLength = dataLength,
    .eTransmitType = EZWAVETRANSMITTYPE_EX
  };
  memcpy(&FramePackage.uTransmitParams.SendDataEx.FrameConfig.aFrame, pData, dataLength);
  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_SEND_DATA_EX)
{
  /* HOST->ZW: nodeID | dataLength | pData[] | txOptions | txSecOptions | securityKey | txOptions2 | funcID */
  /* ZW->HOST: RetVal */
  /* If "RetVal != 1" -> no callback */
  /* If "RetVal == 1" and "funcID != 0" then callback returns with */
  /* ZW->HOST: funcID | txStatus | wTransmitTicksMSB | wTransmitTicksLSB | bRepeaters | rssi_values.incoming[0] | */
  /*           rssi_values.incoming[1] | rssi_values.incoming[2] | rssi_values.incoming[3] | rssi_values.incoming[4] | */
  /*           bRouteSchemeState | repeater0 | repeater1 | repeater2 | repeater3 | routespeed | */
  /*           bRouteTries | bLastFailedLink.from | bLastFailedLink.to */
  uint8_t  offset = 0;
  node_id_t nodeId = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  uint8_t dataLength;

  dataLength = frame->payload[offset + 1];
  assert(dataLength <= BUF_SIZE_RX);
  dataLength = MIN(dataLength, BUF_SIZE_RX);
  funcID_ComplHandler_ZW_SendDataEx = frame->payload[offset + 6 + dataLength];

  const uint8_t retVal = SendDataEx(nodeId, &frame->payload[offset + 2], dataLength, frame->payload[offset + 2 + dataLength],
                      frame->payload[offset + 3 + dataLength], frame->payload[offset + 5 + dataLength],
                      frame->payload[offset + 4 + dataLength], (funcID_ComplHandler_ZW_SendDataEx != 0) ? ZCB_ComplHandler_ZW_SendDataEx : NULL);

  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_SEND_DATA_MULTI || SUPPORT_ZW_SEND_DATA_MULTI_BRIDGE
uint8_t funcID_ComplHandler_ZW_SendDataMulti;
#endif

#if SUPPORT_ZW_SEND_DATA_MULTI
/*=====================   ComplHandler_ZW_SendDataMulti   ====================
**    Completion handler for ZW_SendDataMulti
**
**--------------------------------------------------------------------------*/
static void /* RET  Nothing                     */
ZCB_ComplHandler_ZW_SendDataMulti(
    uint8_t txStatus,
    __attribute__((unused)) TX_STATUS_TYPE *txStatusType) /* IN   Transmit completion status  */
{
  BYTE_IN_AR(compl_workbuf, 0) = funcID_ComplHandler_ZW_SendDataMulti;
  BYTE_IN_AR(compl_workbuf, 1) = txStatus;
  Request(FUNC_ID_ZW_SEND_DATA_MULTI, compl_workbuf, 2);
}

static uint8_t SendDataMulti(uint8_t numberOfNodes, const uint8_t *pNodeList, const uint8_t *pData, uint8_t dataLength, uint8_t txOptions, ZW_TX_Callback_t pCallBack)
{
  // Create transmit frame package
  SZwaveTransmitPackage FramePackage;
  SSendDataMulti *pSendDataMulti = &FramePackage.uTransmitParams.SendDataMulti;
  memset(&pSendDataMulti->FrameConfig.aFrame, 0, sizeof(pSendDataMulti->FrameConfig.aFrame));
  /* clear the destination node mask */
  memset(&pSendDataMulti->NodeMask, 0, sizeof(pSendDataMulti->NodeMask));
  /* Set the destination node mask bits */
  for (uint8_t i = 0; i < numberOfNodes && i < MAX_GROUP_NODES; i++)
  {
    ZW_NodeMaskSetBit(pSendDataMulti->NodeMask, pNodeList[i]);
  }
  assert(dataLength <= BUF_SIZE_RX);
  dataLength = MIN(dataLength, BUF_SIZE_RX);
  pSendDataMulti->FrameConfig.TransmitOptions = txOptions;
  memcpy(&pSendDataMulti->FrameConfig.aFrame, pData, dataLength);
  pSendDataMulti->FrameConfig.Handle = pCallBack;
  FramePackage.eTransmitType = EZWAVETRANSMITTYPE_MULTI;
  FramePackage.uTransmitParams.SendDataMulti.FrameConfig.iFrameLength = dataLength;

  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_SEND_DATA_MULTI)
{
  /* numberNodes | pNodeIDList[] | dataLength | pData[] | txOptions | funcId */
  // Create transmit frame package
  uint8_t numOfNodes = frame->payload[0];
  uint8_t tLength = frame->payload[1 + numOfNodes];
  uint8_t tOptions = frame->payload[2 + numOfNodes + tLength];
  funcID_ComplHandler_ZW_SendDataMulti = frame->payload[3 + numOfNodes + tLength];

  const uint8_t retVal = SendDataMulti(numOfNodes, &frame->payload[1], &frame->payload[2 + numOfNodes], tLength, tOptions,
                          (funcID_ComplHandler_ZW_SendDataMulti != 0) ? &ZCB_ComplHandler_ZW_SendDataMulti : NULL);

  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_SEND_DATA_MULTI_EX
uint8_t funcID_ComplHandler_ZW_SendDataMultiEx;

/*=====================   ComplHandler_ZW_SendDataMulti   ====================
**    Completion handler for ZW_SendDataMulti
**
**--------------------------------------------------------------------------*/
static void
ZCB_ComplHandler_ZW_SendDataMultiEx(
    uint8_t txStatus, /* IN   Transmit completion status  */
    __attribute__((unused)) TX_STATUS_TYPE* extendedTxStatus)
{
  BYTE_IN_AR(compl_workbuf, 0) = funcID_ComplHandler_ZW_SendDataMultiEx;
  BYTE_IN_AR(compl_workbuf, 1) = txStatus;
  Request(FUNC_ID_ZW_SEND_DATA_MULTI_EX, compl_workbuf, 2);
}

static uint8_t SendDataMultiEx(uint8_t dataLength, uint8_t *pData, uint8_t txOptions, uint8_t secKeyType, uint8_t groupID, ZW_TX_Callback_t pCallBack)
{
  assert(dataLength <= BUF_SIZE_RX);
  dataLength = MIN(dataLength, BUF_SIZE_RX);
  // Create transmit frame package
  SZwaveTransmitPackage FramePackage = {
    .uTransmitParams.SendDataMultiEx.FrameConfig.Handle = pCallBack,
    .uTransmitParams.SendDataMultiEx.FrameConfig.TransmitOptions = txOptions,
    .uTransmitParams.SendDataMultiEx.FrameConfig.iFrameLength = dataLength,
    .uTransmitParams.SendDataMultiEx.GroupId = groupID,
    .uTransmitParams.SendDataMultiEx.eKeyType = secKeyType,
    .eTransmitType = EZWAVETRANSMITTYPE_MULTI_EX
  };
  memcpy(&FramePackage.uTransmitParams.SendDataMultiEx.FrameConfig.aFrame, pData, dataLength);
  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_SEND_DATA_MULTI_EX)
{
  /* dataLength | pData[] | txOptions | securityKey | groupId | funcId */
  uint8_t dataLength = frame->payload[0];
  funcID_ComplHandler_ZW_SendDataMultiEx = frame->payload[4 + dataLength];
  uint8_t tOptions = frame->payload[1 + dataLength];
  uint8_t tGID = frame->payload[3 + dataLength];
  uint8_t tKey = frame->payload[2 + dataLength];

  const uint8_t retVal = SendDataMultiEx(dataLength, &frame->payload[1], tOptions, tKey, tGID, (funcID_ComplHandler_ZW_SendDataMultiEx != 0) ? ZCB_ComplHandler_ZW_SendDataMultiEx : NULL);

  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_SEND_DATA_ABORT
static void SendDataAbort(void)
{
  // FIXME - we need to change the implementation of abort
  // What makes sense? aborting based on frame context?
  // We cant peek into FreeRtos queue...
  // We could also reset the queue on abort. and stop any ongoing frame.
  SZwaveCommandPackage CommandPackage = { .eCommandType = EZWAVECOMMANDTYPE_SEND_DATA_ABORT };
  // Put the package on queue (and DO wait for it, since there is no feedback to serial master)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&CommandPackage, 500);

  if (EQUEUENOTIFYING_STATUS_SUCCESS != QueueStatus)
  {
    DPRINT("Warning: Failed to To call ZW_SendDataAbort");
  }
}

ZW_ADD_CMD(FUNC_ID_ZW_SEND_DATA_ABORT)
{
  /* If we are in middle of transmitting an application frame then STOP the transmission as soon as possible. */
  SendDataAbort();
  set_state_and_notify(stateIdle);
}
#endif


#if SUPPORT_ZW_SEND_DATA_BRIDGE
/*=================   ComplHandler_ZW_SendData_Bridge   ======================
**    Completion handler for ZW_SendData_Bridge
**
**--------------------------------------------------------------------------*/
static void /* RET  Nothing                     */
ZCB_ComplHandler_ZW_SendData_Bridge(
    uint8_t txStatus,
    TX_STATUS_TYPE *txStatusReport) /* IN   Transmit completion status  */
{
  GenerateTxStatusRequest(FUNC_ID_ZW_SEND_DATA_BRIDGE, funcID_ComplHandler_ZW_SendData, txStatus, txStatusReport);
}

static uint8_t SendDataBridge(uint16_t srcNode, uint16_t destNode, uint8_t dataLength, const uint8_t *pData, uint8_t txOptions, ZW_TX_Callback_t pCallBack)
{
  assert(dataLength <= BUF_SIZE_RX);
  dataLength = MIN(dataLength, BUF_SIZE_RX);
  // Create transmit frame package
  SZwaveTransmitPackage FramePackage = {
    .uTransmitParams.SendDataBridge.FrameConfig.Handle = pCallBack,
    .uTransmitParams.SendDataBridge.FrameConfig.TransmitOptions = txOptions,
    .uTransmitParams.SendDataBridge.FrameConfig.iFrameLength = dataLength,
    .uTransmitParams.SendDataBridge.DestNodeId = destNode,
    .uTransmitParams.SendDataBridge.SourceNodeId = srcNode,
    .eTransmitType = EZWAVETRANSMITTYPE_BRIDGE
  };
  memcpy(&FramePackage.uTransmitParams.SendDataBridge.FrameConfig.aFrame, pData, dataLength);
  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_SEND_DATA_BRIDGE)
{
  /* HOST->ZW: srcNodeID | destNodeID | dataLength | pData[] | txOptions | pRoute[4] | funcID */
  /* Devkit 6.0x pRoute[4] not used... Use [0,0,0,0] */
  uint8_t  offset = 0;
  node_id_t sourceNodeId;
  node_id_t destNodeId;
  sourceNodeId = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  destNodeId   = (node_id_t)GET_NODEID(&frame->payload[1 + offset], offset);
  uint8_t dataLength = frame->payload[offset + 2];
  funcID_ComplHandler_ZW_SendData = frame->payload[offset + 3 + 1 + 4 + dataLength];
  uint8_t tOptions = frame->payload[offset + 3 + dataLength];
  const uint8_t retVal = SendDataBridge(sourceNodeId, destNodeId, dataLength, &frame->payload[offset + 3], tOptions,
                          (funcID_ComplHandler_ZW_SendData != 0) ? &ZCB_ComplHandler_ZW_SendData_Bridge : NULL);

  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_SEND_DATA_MULTI_BRIDGE
/*================   ComplHandler_ZW_SendDataMulti_Bridge   ==================
**    Completion handler for ZW_SendDataMulti
**
**--------------------------------------------------------------------------*/
static void /* RET  Nothing                     */
ZCB_ComplHandler_ZW_SendDataMulti_Bridge(
    uint8_t txStatus, /* IN   Transmit completion status  */
    __attribute__((unused)) TX_STATUS_TYPE* extendedTxStatus)
{
  BYTE_IN_AR(compl_workbuf, 0) = funcID_ComplHandler_ZW_SendDataMulti;
  BYTE_IN_AR(compl_workbuf, 1) = txStatus;
  Request(FUNC_ID_ZW_SEND_DATA_MULTI_BRIDGE, compl_workbuf, 2);
}

static uint8_t SendDataMultiBridge(node_id_t srcNode, uint8_t numOfNodes, uint8_t *pNodeIDList,
                                   uint8_t dataLength, const uint8_t *pData, uint8_t txOptions, ZW_TX_Callback_t pCallBack)
{
  // when nodeIdBaseType is 2 then we handle the FramePackage.uTransmitParams.SendDataMultiBridge.NodeMask as node list
  // when nodeIdBaseType is 1 then we handle the FramePackage.uTransmitParams.SendDataMultiBridge.NodeMask as node mask
  // Create transmit frame package
  assert(dataLength <= BUF_SIZE_RX);
  dataLength = MIN(dataLength, BUF_SIZE_RX);

  SZwaveTransmitPackage FramePackage = {
    .uTransmitParams.SendDataMultiBridge.FrameConfig.Handle = pCallBack,
    .uTransmitParams.SendDataMultiBridge.FrameConfig.TransmitOptions = txOptions,
    .uTransmitParams.SendDataMultiBridge.FrameConfig.iFrameLength = dataLength,
    .uTransmitParams.SendDataMultiBridge.SourceNodeId = srcNode,
    .eTransmitType = EZWAVETRANSMITTYPE_MULTI_BRIDGE
   };

  memcpy(&FramePackage.uTransmitParams.SendDataMultiBridge.FrameConfig.aFrame, pData, dataLength);

  bool lr_list = false;
  bool classic_list = false;

  if (SERIAL_API_SETUP_NODEID_BASE_TYPE_16_BIT == nodeIdBaseType) {
    /*if nodeIdBaseType 2 then we should check if the list contain lr only nodes or classic only nodes mixed list is not allowed*/
    if ((MULTICAST_NODE_LIST_SIZE>>1) < numOfNodes) {
       /*maximum allowed nodes are 32 */
      numOfNodes = MULTICAST_NODE_LIST_SIZE>>1;
    }
    for (uint8_t i = 0; i < numOfNodes; i++)
    {
      node_id_t curNode =  (node_id_t) (((node_id_t)pNodeIDList[i<<1] << 8) |    // index = i *2
                                        (pNodeIDList[(i<<1) +1] & 0xFF));        // index = (i *2) +1

      if ((LOWEST_LONG_RANGE_NODE_ID <= curNode) && (HIGHEST_LONG_RANGE_NODE_ID >= curNode)) {
        lr_list = true;
      } else {
        classic_list = true;
      }
      if (lr_list && classic_list) {
        /* mixed list we bail out*/
        return false;
      }
    }
    if (lr_list)  {
      /*lr nodes are 16-bit and the list is in bytes then we copy 2 * numOfNodes*/
      memcpy(FramePackage.uTransmitParams.SendDataMultiBridge.NodeMask, pNodeIDList , numOfNodes * 2);
    }
  } else {
    classic_list = true;
    if (MAX_GROUP_NODES < numOfNodes) {
      numOfNodes = MAX_GROUP_NODES;
    }
  }

  FramePackage.uTransmitParams.SendDataMultiBridge.lr_nodeid_list = lr_list;
  if (classic_list) {
    uint8_t tmpNode;
    for (uint8_t i = 0; i < numOfNodes; i++)
    {
      // if the list of classic nodes then the node ID is located in the lsb bytes of the 16-bit nodeID list.
      if (SERIAL_API_SETUP_NODEID_BASE_TYPE_16_BIT == nodeIdBaseType) {
        tmpNode = pNodeIDList[(i<<1) +1];
      } else {
        /* Set the destination node mask bits */
        tmpNode = pNodeIDList[i];
      }
      ZW_NodeMaskSetBit(FramePackage.uTransmitParams.SendDataMultiBridge.NodeMask, tmpNode);
    }
  }
  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_SEND_DATA_MULTI_BRIDGE)
{
  /* HOST->ZW: srcNodeID | numberNodes | pNodeIDList[] | dataLength | pData[] | txOptions | funcId */
  uint8_t   numberNodes;
  uint8_t   dataLength;
  uint8_t   txOptions;
  uint8_t   offset = 0;
  node_id_t   srcNodeId = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  uint8_t   nodeid_list_size;

  numberNodes = frame->payload[offset + 1];
  uint8_t *pNodeList = &frame->payload[offset + 2];

  if (nodeIdBaseType == SERIAL_API_SETUP_NODEID_BASE_TYPE_16_BIT) {
    nodeid_list_size = numberNodes * 2;
  } else {
    nodeid_list_size = numberNodes;
  }

  dataLength = frame->payload[offset + 2 + nodeid_list_size];
  txOptions = frame->payload[offset + 2 + 1 + nodeid_list_size + dataLength];
  funcID_ComplHandler_ZW_SendDataMulti = frame->payload[offset + 2 + 1 + 1 + nodeid_list_size + dataLength];
  uint8_t *pDataBuf = &frame->payload[offset + 3 + nodeid_list_size];

  const uint8_t retVal = SendDataMultiBridge(srcNodeId, numberNodes, pNodeList,
                                dataLength, pDataBuf, txOptions,
                                (funcID_ComplHandler_ZW_SendDataMulti != 0) ? &ZCB_ComplHandler_ZW_SendDataMulti_Bridge : NULL);

  DoRespond(retVal);
}
#endif


#if SUPPORT_MEMORY_GET_ID
ZW_ADD_CMD(FUNC_ID_MEMORY_GET_ID)
{
  uint8_t i = 0;
  node_id_t node_id;
  uint32_t home_id;

  node_id = ZAF_GetNodeID();
  home_id = ZAF_GetHomeID();

  /*  */
  compl_workbuf[i++] = (uint8_t) ((home_id & 0xff000000) >> 24);
  compl_workbuf[i++] = (uint8_t) ((home_id & 0x00ff0000) >> 16);
  compl_workbuf[i++] = (uint8_t) ((home_id & 0x0000ff00) >> 8);
  compl_workbuf[i++] = (uint8_t)  (home_id & 0x000000ff);
  if (SERIAL_API_SETUP_NODEID_BASE_TYPE_16_BIT == nodeIdBaseType)
  {
    // 16 bit nodeID
    compl_workbuf[i++] = (uint8_t)(node_id >> 8);  // MSB(16bit)
  }
  compl_workbuf[i++] = (uint8_t)(node_id & 0xFF);  // LSB(16bit)/8bit
  DoRespond_workbuf(i);
}
#endif


#if SUPPORT_MEMORY_GET_BYTE
ZW_ADD_CMD(FUNC_ID_MEMORY_GET_BYTE)
{
  /* offset (MSB) | offset (LSB) */
  uint8_t retVal = 0;
  if ((FRAME_LENGTH_MIN + 2) < frame->len)
  {
    uint16_t offset =  ((uint16_t)(frame->payload[0] << 8)) + frame->payload[1];
    if (! SerialApiNvmReadAppData(offset, &retVal, 1))
    {
      retVal = 0;
    }
  }
  DoRespond(retVal);
}
#endif


#if SUPPORT_MEMORY_PUT_BYTE
ZW_ADD_CMD(FUNC_ID_MEMORY_PUT_BYTE)
{
  uint8_t retVal = 0;
  if ((FRAME_LENGTH_MIN + 3) < frame->len)
  {
    uint16_t offset =  ((uint16_t)(frame->payload[0] << 8)) + frame->payload[1];
    retVal = SerialApiNvmWriteAppData(offset, &frame->payload[2], 1);
    }
    DoRespond(retVal);
}
#endif


#if SUPPORT_MEMORY_GET_BUFFER
ZW_ADD_CMD(FUNC_ID_MEMORY_GET_BUFFER)
{
  uint8_t dataLength = 0;
  dataLength = frame->payload[2];
    /* Make sure the length isn't larger than the available buffer size */
  if (dataLength > (uint8_t)BUF_SIZE_TX)
  {
    dataLength = (uint8_t)BUF_SIZE_TX;
  }
  uint16_t offset =  ((uint16_t)(frame->payload[0] << 8)) + frame->payload[1];
  if (!SerialApiNvmReadAppData(offset, compl_workbuf, dataLength))
  {
    dataLength = 0;
  }
  DoRespond_workbuf(dataLength);
}
#endif


#if SUPPORT_MEMORY_PUT_BUFFER
uint8_t funcID_ComplHandler_MemoryPutBuffer;

/*=====================   ComplHandler_MemoryPutBuffer   =============
**    Completion handler for MemoryPutBuffer
**
**--------------------------------------------------------------------------*/
static void                                /* RET  Nothing */
ZCB_ComplHandler_MemoryPutBuffer(void)  /* IN   Nothing */
{
  BYTE_IN_AR(compl_workbuf, 0) = funcID_ComplHandler_MemoryPutBuffer;
  Request(FUNC_ID_MEMORY_PUT_BUFFER, compl_workbuf, 1);
}

ZW_ADD_CMD(FUNC_ID_MEMORY_PUT_BUFFER)
{
    /* HOST->ZW:
        offset(MSB)        offset into host application NVM memory array
        offset(LSB)
        length(MSB)        desired length of write operation
        length(LSB)
        buffer[]           buffer
        funcID
      */
    /* ZW->HOST:
        retVal             [retVal=0 ==> error|
                            retVal=1 ==> OK (NVM no change)|
                            retVal>=2 ==> OK (NVM data bytes written + 1)]
      */
  uint16_t length;
  uint8_t retVal = 0;
  ///* Ignore if frame has no data to write */
  length = ((uint16_t)(frame->payload[2] << 8)) + frame->payload[3];
    /* Ignore write if length exceeds specified data-array */
  if (length < BUF_SIZE_RX )
  {
    /* ignore request if length is larger than available buffer */
    if (length < BUF_SIZE_RX)
    {
      const uint8_t * const pSerInData = frame->payload + 4;
      uint16_t offset =  ((uint16_t)(frame->payload[0] << 8)) + frame->payload[1];
      retVal = SerialApiNvmWriteAppData(offset, pSerInData, length);
    }
  }
  DoRespond(retVal);
  funcID_ComplHandler_MemoryPutBuffer = frame->payload[4 + length];
  if ((0 != retVal) && (0 != funcID_ComplHandler_MemoryPutBuffer) )
  {
    ZCB_ComplHandler_MemoryPutBuffer();
  }
}
#endif


#if SUPPORT_NVM_BACKUP_RESTORE
ZW_ADD_CMD(FUNC_ID_NVM_BACKUP_RESTORE)
{
  if (true == NvmBackupLegacyCmdAvailable())
  {
    uint8_t length = 0;
    func_id_serial_api_nvm_backup_restore(frame_payload_len(frame), frame->payload, compl_workbuf, &length, false);
    DoRespond_workbuf(length);
  }
  else
  {
    //if legacy command is not allowed, drop it
    set_state_and_notify(stateIdle);
  }
}
#endif

#if SUPPORT_NVM_EXT_BACKUP_RESTORE
ZW_ADD_CMD(FUNC_ID_NVM_EXT_BACKUP_RESTORE)
{
  uint8_t length = 0;
  func_id_serial_api_nvm_backup_restore(frame_payload_len(frame), frame->payload, compl_workbuf, &length, true);
  DoRespond_workbuf(length);
}
#endif


#if SUPPORT_NVM_GET_ID
ZW_ADD_CMD(FUNC_ID_NVM_GET_ID)
{
}
#endif


#if SUPPORT_NVM_EXT_READ_LONG_BYTE
ZW_ADD_CMD(FUNC_ID_NVM_EXT_READ_LONG_BYTE)
{
  /* HOST->ZW: offset3byte(MSB) | offset3byte | offset3byte(LSB) */
  /* ZW->HOST: dataread */
  uint8_t retVal = 0;
  if ((FRAME_LENGTH_MIN + 2) < frame->len)
  {
    uint32_t offset = (((uint32_t)frame->payload[0] << 16) + ((uint32_t)((uint16_t)frame->payload[1] << 8)) + frame->payload[2]);
    if (! SerialApiNvmReadAppData(offset, &retVal, 1))
    {
      retVal = 0;
    }
  }
  DoRespond(retVal);
}
#endif


#if SUPPORT_NVM_EXT_WRITE_LONG_BYTE
ZW_ADD_CMD(FUNC_ID_NVM_EXT_WRITE_LONG_BYTE)
{
  /* HOST->ZW: offset3byte(MSB) | offset3byte | offset3byte(LSB) | data */
  /* ZW->HOST: writestatus */
  uint8_t retVal = 0;
  if ((FRAME_LENGTH_MIN + 3) < frame->len)
  {
    uint32_t offset = (((uint32_t)frame->payload[0] << 16) + ((uint32_t)((uint16_t)frame->payload[1] << 8)) + frame->payload[2]);
    retVal = SerialApiNvmWriteAppData(offset, &frame->payload[3], 1);
  }
  DoRespond(retVal);
}
#endif


#if SUPPORT_NVM_EXT_READ_LONG_BUFFER
ZW_ADD_CMD(FUNC_ID_NVM_EXT_READ_LONG_BUFFER)
{
  /* HOST->ZW: offset3byte(MSB) | offset3byte | offset3byte(LSB) | length2byte(MSB) | length2byte(LSB) */
  /* ZW->HOST: data[] */
  uint16_t dataLength = 0;
  ///* Ignore if frame is to short */
  if ((FRAME_LENGTH_MIN + 3 + 1) < frame->len)
  {
    dataLength = ((uint16_t)(frame->payload[3] << 8)) + frame->payload[4];
    /* Make sure the length isn't larger than the available buffer size */
    if (dataLength > (uint8_t)BUF_SIZE_TX)
    {
      dataLength = (uint8_t)BUF_SIZE_TX;
    }
    uint32_t offset = (((uint32_t)frame->payload[0] << 16) + ((uint32_t)((uint16_t)frame->payload[1] << 8)) + frame->payload[2]);
    if (!SerialApiNvmReadAppData(offset, compl_workbuf, dataLength))
    {
      dataLength = 0;
    }
  }
  DoRespond_workbuf((uint8_t)dataLength);
}
#endif


#if SUPPORT_NVM_EXT_WRITE_LONG_BUFFER
ZW_ADD_CMD(FUNC_ID_NVM_EXT_WRITE_LONG_BUFFER)
{
  /* HOST->ZW: offset3byte(MSB) | offset3byte | offset2byte(LSB) | length2byte(MSB) | length2byte(LSB) | buffer[] */
  /* ZW->HOST: retVal */
  uint8_t retVal = 0;
  ///* Ignore if frame has no data to write */
  if ((FRAME_LENGTH_MIN + 5) < frame->len)
  {
    uint16_t length;
    length = ((uint16_t)(frame->payload[3] << 8)) + frame->payload[4];
    /* Ignore write if length exceeds specified data-array */
    if (length <= frame->len - FRAME_LENGTH_MIN)
    {
      /* ignore request if length is larger than available buffer */
      if (length < BUF_SIZE_RX)
      {
        const uint8_t * const pSerInData = frame->payload + 5;
        uint32_t offset = (((uint32_t)frame->payload[0] << 16) + ((uint32_t)((uint16_t)frame->payload[1] << 8)) + frame->payload[2]);
        retVal = SerialApiNvmWriteAppData(offset, pSerInData, length);
      }
    }
  }
  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_NVR_GET_VALUE
ZW_ADD_CMD(FUNC_ID_NVR_GET_VALUE)
{
  /* HOST->ZW: offset | length */
  /* ZW->HOST: NVRdata[] */
  uint8_t dataLength = 0;
  /* Ignore if frame is too short */
  if ((FRAME_LENGTH_MIN + 1) < frame->len)
  {
    /*inputLength paramter is nout used*/
    func_id_serial_api_get_nvr(0, frame->payload, compl_workbuf, &dataLength);
  }
  DoRespond_workbuf(dataLength);
}
#endif


#if SUPPORT_SERIAL_API_GET_APPL_HOST_MEMORY_OFFSET
ZW_ADD_CMD(FUNC_ID_SERIAL_API_GET_APPL_HOST_MEMORY_OFFSET)
{
  DoRespond(0);
}
#endif


#if defined(ZW_SLAVE_ROUTING) || defined(ZW_CONTROLLER)
uint8_t funcID_ComplHandler_netWork_Management;
uint8_t management_Func_ID;

/*=====================   ComplHandler_ZW_netWork_Management   ===============
**    Completion handler for the network management functionality
**
**--------------------------------------------------------------------------*/
static void /* RET  Nothing                     */
ZCB_ComplHandler_ZW_netWork_Management(
    uint8_t bStatus,                   /* IN   Transmit completion status  */
    TX_STATUS_TYPE *txStatusReport) /* IN Detailed transmit information */
{
  uint8_t bIdx = 0;
  BYTE_IN_AR(compl_workbuf, bIdx++) = funcID_ComplHandler_netWork_Management;
  BYTE_IN_AR(compl_workbuf, bIdx++) = bStatus;
  if (bTxStatusReportEnabled && txStatusReport) /* Check if detailed info is available from protocol */
  {
    memcpy(&compl_workbuf[bIdx], (uint8_t *)txStatusReport, sizeof(TX_STATUS_TYPE));
    bIdx += sizeof(TX_STATUS_TYPE);
  }
  Request(management_Func_ID, compl_workbuf, bIdx);
}
#endif /*ZW_SLAVE_32 ZW_CONTROLLER*/


#if SUPPORT_ZW_REQUEST_NETWORK_UPDATE
static uint8_t RequestNetworkUpdate(ZW_TX_Callback_t pCallBack)
{
  // Create transmit frame package
  SZwaveTransmitPackage FramePackage = {
    .eTransmitType = EZWAVETRANSMITTYPE_NETWORKUPDATEREQUEST,
    .uTransmitParams.NetworkUpdateRequest.Handle = (ZW_Void_Callback_t)pCallBack
  };
  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_REQUEST_NETWORK_UPDATE)
{
  /* funcID */
  funcID_ComplHandler_netWork_Management = frame->payload[0];
  management_Func_ID = frame->cmd;
  const uint8_t retVal = RequestNetworkUpdate((frame->payload[0] != 0) ? &ZCB_ComplHandler_ZW_netWork_Management : NULL);
  DoRespond(retVal);
}
#endif /* SUPPORT_ZW_REQUEST_NETWORK_UPDATE */


#if SUPPORT_ZW_REQUEST_NODE_NEIGHBOR_UPDATE
uint8_t funcID_ComplHandler_ZW_RequestNodeNeighborUpdate;

/*===============   ComplHandler_ZW_RequestNodeNeighborUpdate  ===============
**    Completion handler for ZW_REQUEST_NODE_NEIGHBOR_UPDATE
**
**--------------------------------------------------------------------------*/
static void /* RET  Nothing                    */
ZCB_ComplHandler_ZW_RequestNodeNeighborUpdate(
    uint8_t txStatus, /* IN   Transmit completion status */
    __attribute__((unused)) TX_STATUS_TYPE *txStatusReport)
{
  BYTE_IN_AR(compl_workbuf, 0) = funcID_ComplHandler_ZW_RequestNodeNeighborUpdate;
  BYTE_IN_AR(compl_workbuf, 1) = txStatus;
  Request(FUNC_ID_ZW_REQUEST_NODE_NEIGHBOR_UPDATE, compl_workbuf, 2);
}

static uint8_t RequestNodeNeighborUpdate(uint16_t nodeID, ZW_TX_Callback_t pCallBack)
{

    SZwaveCommandPackage Request = {
      .eCommandType = EZWAVECOMMANDTYPE_REQUESTNODENEIGHBORUPDATE,
      .uCommandParams.RequestNodeNeighborUpdate.NodeId = nodeID,
      .uCommandParams.RequestNodeNeighborUpdate.Handle = (ZW_Void_Callback_t)pCallBack};

  // Put the Command on queue (and dont wait for it, queue must be empty)
  if (EQUEUENOTIFYING_STATUS_SUCCESS == QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&Request, 0))
  {
    // Wait for protocol to handle command
    SZwaveCommandStatusPackage status = { .eStatusType = EZWAVECOMMANDSTATUS_ZW_REQUESTNODENEIGHBORUPDATE};
    if (GetCommandResponse(&status, status.eStatusType))
    {
      return  status.Content.RequestNodeNeigborUpdateStatus.result;
    }
  }
  return false;
}

ZW_ADD_CMD(FUNC_ID_ZW_REQUEST_NODE_NEIGHBOR_UPDATE)
{
  uint8_t offset = 0;
  node_id_t nodeId = (node_id_t)GET_NODEID(&frame->payload[0], offset);

  funcID_ComplHandler_ZW_RequestNodeNeighborUpdate = frame->payload[1 + offset];

  // Put the package on queue (and dont wait for it)
  if (!RequestNodeNeighborUpdate(nodeId,
                                  funcID_ComplHandler_ZW_RequestNodeNeighborUpdate ? &ZCB_ComplHandler_ZW_RequestNodeNeighborUpdate : NULL))
  {
    ZCB_ComplHandler_ZW_RequestNodeNeighborUpdate(REQUEST_NEIGHBOR_UPDATE_FAILED, NULL);
  }
  set_state_and_notify(stateIdle);
}

uint8_t funcID_ComplHandler_ZW_RequestNodeTypeNeighborUpdate;

/*===============   ComplHandler_ZW_RequestNodeTypeNeighborUpdate  ===============
**    Completion handler for ZW_REQUEST_NODE_NEIGHBOR_UPDATE
**
**--------------------------------------------------------------------------*/
static void /* RET  Nothing                    */
ZCB_ComplHandler_ZW_RequestNodeTypeNeighborUpdate(
    uint8_t txStatus, /* IN   Transmit completion status */
    __attribute__((unused)) TX_STATUS_TYPE *txStatusReport)
{
  BYTE_IN_AR(compl_workbuf, 0) = funcID_ComplHandler_ZW_RequestNodeTypeNeighborUpdate;
  BYTE_IN_AR(compl_workbuf, 1) = txStatus;
  Request(FUNC_ID_ZW_REQUEST_NODETYPE_NEIGHBOR_UPDATE, compl_workbuf, 2);
}

static uint8_t RequestNodeTypeNeighborUpdate(uint16_t nodeID, uint8_t nodeType, ZW_TX_Callback_t pCallBack)
{

    SZwaveCommandPackage Request = {
      .eCommandType = EZWAVECOMMANDTYPE_REQUESTNODETYPE_NEIGHBORUPDATE,
      .uCommandParams.RequestNodeTypeNeighborUpdate.NodeId = nodeID,
      .uCommandParams.RequestNodeTypeNeighborUpdate.NodeType = nodeType,
      .uCommandParams.RequestNodeTypeNeighborUpdate.Handle = (ZW_Void_Callback_t)pCallBack};

  // Put the Command on queue (and dont wait for it, queue must be empty)
  if (EQUEUENOTIFYING_STATUS_SUCCESS == QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&Request, 0))
  {
    // Wait for protocol to handle command
    SZwaveCommandStatusPackage status = { 0 };
    if (GetCommandResponse(&status, EZWAVECOMMANDSTATUS_ZW_REQUESTNODETYPE_NEIGHBORUPDATE))
    {
      return  status.Content.RequestNodeTypeNeigborUpdateStatus.result;
    }
  }
  return false;
}

ZW_ADD_CMD(FUNC_ID_ZW_REQUEST_NODETYPE_NEIGHBOR_UPDATE)
{
  uint8_t offset = 0;
  E_SYSTEM_TYPE nodeType = 0;
  node_id_t nodeId = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  nodeType = frame->payload[1 + offset];
  ++offset;

  funcID_ComplHandler_ZW_RequestNodeTypeNeighborUpdate = frame->payload[1 + offset];

  // Put the package on queue (and dont wait for it)
  if (!RequestNodeTypeNeighborUpdate(nodeId, nodeType,
                                  funcID_ComplHandler_ZW_RequestNodeTypeNeighborUpdate ? &ZCB_ComplHandler_ZW_RequestNodeTypeNeighborUpdate : NULL))
  {
    ZCB_ComplHandler_ZW_RequestNodeTypeNeighborUpdate(REQUEST_NEIGHBOR_UPDATE_FAILED, NULL);
  }
  set_state_and_notify(stateIdle);
}
#endif


#if SUPPORT_ZW_GET_NODE_PROTOCOL_INFO
ZW_ADD_CMD(FUNC_ID_ZW_GET_NODE_PROTOCOL_INFO)
{
  /* bNodeID */
  volatile uint8_t offset = 0;
  node_id_t nodeId = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  GetNodeInfo(nodeId, (t_ExtNodeInfo *)compl_workbuf);
  _Static_assert(sizeof(t_ExtNodeInfo) == 7, "STATIC_ASSERT_FAILED_size_mismatch");
  DoRespond_workbuf(7);
}
#endif


#if SUPPORT_ZW_SET_DEFAULT
uint8_t funcID_ComplHandler_ZW_SetDefault;

/*=====================   ComplHandler_ZW_SetDefault   =============
**    Completion handler for ZW_SetDefault
**
**--------------------------------------------------------------------------*/
static void                          /* RET  Nothing */
ZCB_ComplHandler_ZW_SetDefault(void) /* IN   Nothing */
{
  BYTE_IN_AR(compl_workbuf, 0) = funcID_ComplHandler_ZW_SetDefault;
  Request(FUNC_ID_ZW_SET_DEFAULT, compl_workbuf, 1);
}

static void SetDefault(ZW_Void_Callback_t pCallBack)
{
  /* funcID */
  SyncEventUnbind(&SetDefaultCB);
  if (0 != pCallBack)
  {
    SyncEventBind(&SetDefaultCB, pCallBack);
  }
  SZwaveCommandPackage CommandPackage = { .eCommandType = EZWAVECOMMANDTYPE_SET_DEFAULT };
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&CommandPackage, 500);
  if (EQUEUENOTIFYING_STATUS_SUCCESS != QueueStatus)
  {
    DPRINT("Warning: Failed to perform SetDefault");
  }
}

ZW_ADD_CMD(FUNC_ID_ZW_SET_DEFAULT)
{
  /* funcID */
  funcID_ComplHandler_ZW_SetDefault = frame->payload[0];
  SetDefault((0 != funcID_ComplHandler_ZW_SetDefault) ? &ZCB_ComplHandler_ZW_SetDefault : NULL);
  set_state_and_notify(stateIdle);
}
#endif

#ifdef ZW_CONTROLLER
/*=======================   ComplHandler_ZW_NodeManagement   =================
**    Completion handler for ZW_AddNodeToNetwork, ZW_RemoveNodeFromNetwork
**    ZW_CreateNewPrimary, ZW_ControllerChange and ZW_SetLearnMode for
**    controller based applications
**
**--------------------------------------------------------------------------*/

void /* RET  Nothing */
ZCB_ComplHandler_ZW_NodeManagement(
    LEARN_INFO_T *statusInfo)
{
  if (0 == funcID_ComplHandler_ZW_NodeManagement)
  {
    return;
  }

  uint8_t offset = 0;
  addState = statusInfo->bStatus;
  BYTE_IN_AR(compl_workbuf, 0) = funcID_ComplHandler_ZW_NodeManagement;
  BYTE_IN_AR(compl_workbuf, 1) = (*statusInfo).bStatus;
  if (SERIAL_API_SETUP_NODEID_BASE_TYPE_16_BIT == nodeIdBaseType)
  {
    BYTE_IN_AR(compl_workbuf, 2) = (uint8_t)(statusInfo->bSource >> 8); // MSB
    BYTE_IN_AR(compl_workbuf, 3) = (uint8_t)(statusInfo->bSource & 0xFF);      // LSB
    offset++;  // 16 bit nodeID means the command fields that follow are offset by one byte
  }
  else
  {
    BYTE_IN_AR(compl_workbuf, 2) = (uint8_t)(statusInfo->bSource & 0xFF);      // Legacy 8 bit nodeID
  }
  /*  - Buffer boundary check */
  if (statusInfo->bLen > (uint8_t)(BUF_SIZE_TX - (offset + 4)))
  {
    statusInfo->bLen = (uint8_t)(BUF_SIZE_TX - (offset + 4));
  }
  BYTE_IN_AR(compl_workbuf, offset + 3) = statusInfo->bLen;
  if(statusInfo->pCmd != NULL)
  {
    for (uint8_t i = 0; i < statusInfo->bLen; i++)
    {
      BYTE_IN_AR(compl_workbuf, offset + 4 + i) = statusInfo->pCmd[i];
    }
  }
  Request(nodeManagement_Func_ID, compl_workbuf, (uint8_t)(offset + statusInfo->bLen + 4));
}

bool ZW_NodeManagementRunning(void)
{
  return (addState == ADD_NODE_STATUS_NODE_FOUND ||
          addState == ADD_NODE_STATUS_ADDING_SLAVE ||
          addState == ADD_NODE_STATUS_ADDING_CONTROLLER);
}
#endif


#if SUPPORT_ZW_ADD_NODE_TO_NETWORK
static void AddNodeToNetwork(uint8_t mode, void (*pCallBack)(LEARN_INFO_T *statusInfo))
{
  SZwaveCommandPackage pCmdPackage = {
    .eCommandType = EZWAVECOMMANDTYPE_ADD_NODE_TO_NETWORK,
    .uCommandParams.NetworkManagement.mode = mode,
    .uCommandParams.NetworkManagement.pHandle = (ZW_Void_Callback_t)pCallBack
                                      };
  // Put the package on queue (and dont wait for it)
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&pCmdPackage, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
}

static void AddNodeDskToNetwork(uint8_t mode, const uint8_t* pDsk, void (*pCallBack)(LEARN_INFO_T *statusInfo))
{
  SZwaveCommandPackage pCmdPackage = {
    .eCommandType = EZWAVECOMMANDTYPE_ADD_NODE_DSK_TO_NETWORK,
    .uCommandParams.NetworkManagementDSK.mode = mode,
    .uCommandParams.NetworkManagementDSK.pHandle = (ZW_Void_Callback_t)pCallBack
                                      };
  memcpy(&pCmdPackage.uCommandParams.NetworkManagementDSK.dsk[0], pDsk, 8);
  // Put the package on queue (and dont wait for it)
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&pCmdPackage, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
}

ZW_ADD_CMD(FUNC_ID_ZW_ADD_NODE_TO_NETWORK)
{
  /* HOST->ZW: mode | funcID */
  /* HOST->ZW: mode = 0x07 | funcID | DSK[0] | DSK[1] | DSK[2] | DSK[3] | DSK[4] | DSK[5] | DSK[6] | DSK[7] */
  if (ZW_NodeManagementRunning() && ((frame->payload[0] & ADD_NODE_MODE_MASK) != ADD_NODE_STOP))
  {
    // A previous node management request is still in progress. Drop this request and go back to idle state.
    set_state_and_notify(stateIdle);
    return;
  }
  SetupNodeManagement(frame, 1);
  if ((frame->payload[0] & ADD_NODE_MODE_MASK) == ADD_NODE_HOME_ID)
  {
    AddNodeDskToNetwork(frame->payload[0],
                        &frame->payload[2],
                        (funcID_ComplHandler_ZW_NodeManagement != 0) ? &ZCB_ComplHandler_ZW_NodeManagement : NULL);
  }
  else
  {
    AddNodeToNetwork(frame->payload[0],
                      (funcID_ComplHandler_ZW_NodeManagement != 0) ? &ZCB_ComplHandler_ZW_NodeManagement : NULL);
  }
}
#endif

#if defined (SUPPORT_ZW_REMOVE_NODE_ID_FROM_NETWORK) && (SUPPORT_ZW_REMOVE_NODE_ID_FROM_NETWORK == 1)
static void RemoveNodeFromNetwork(uint8_t mode, node_id_t node_id, void (*pCallBack)(LEARN_INFO_T *statusInfo))
{
   SZwaveCommandPackage pCmdPackage = {
    .eCommandType = EZWAVECOMMANDTYPE_REMOVE_NODE_FROM_NETWORK,
    .uCommandParams.NetworkManagement.mode = mode,
    .uCommandParams.NetworkManagement.pHandle = (ZW_Void_Callback_t)pCallBack
   };

  if (0 != node_id) {
    pCmdPackage.eCommandType = EZWAVECOMMANDTYPE_REMOVE_NODEID_FROM_NETWORK;
    pCmdPackage.uCommandParams.NetworkManagement.nodeID = node_id;
  }
  // Put the package on queue (and dont wait for it)
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&pCmdPackage, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);

}
#endif

#if SUPPORT_ZW_REMOVE_NODE_ID_FROM_NETWORK
ZW_ADD_CMD(FUNC_ID_ZW_REMOVE_NODE_FROM_NETWORK)
{
  /* HOST->ZW: mode | funcID */
  if (ZW_NodeManagementRunning())
  {
    // A previous node management request is still in progress. Drop this request and go back to idle state.
    set_state_and_notify(stateIdle);
    return;
  }
  SetupNodeManagement(frame, 1);
  RemoveNodeFromNetwork(frame->payload[0], 0,
                        (funcID_ComplHandler_ZW_NodeManagement != 0) ? &ZCB_ComplHandler_ZW_NodeManagement : NULL);
}
#endif


#ifdef ZW_CONTROLLER
ZW_ADD_CMD(FUNC_ID_ZW_REMOVE_NODE_ID_FROM_NETWORK)
{
  /* HOST->ZW: mode | nodeID | funcID */
  uint8_t offset = 0;
  uint16_t nodeId = (uint16_t)GET_NODEID(&frame->payload[1], offset);
  if (ZW_NodeManagementRunning())
  {
    // A previous node management request is still in progress. Drop this request and go back to idle state.
    set_state_and_notify(stateIdle);
    return;
  }
  SetupNodeManagement(frame, offset + 2);

  RemoveNodeFromNetwork(frame->payload[0], nodeId,
                        (funcID_ComplHandler_ZW_NodeManagement != 0) ? &ZCB_ComplHandler_ZW_NodeManagement : NULL);
}
#endif


#if SUPPORT_ZW_CONTROLLER_CHANGE
static void ControllerChange(uint8_t mode, void (*pCallBack)(LEARN_INFO_T *statusInfo))
{
  SZwaveCommandPackage pCmdPackage = {
      .eCommandType = EZWAVECOMMANDTYPE_CONTROLLER_CHANGE,
      .uCommandParams.NetworkManagement.mode = mode,
      .uCommandParams.NetworkManagement.pHandle = (ZW_Void_Callback_t)pCallBack};

  // Put the package on queue (and dont wait for it)
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&pCmdPackage, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
}

ZW_ADD_CMD(FUNC_ID_ZW_CONTROLLER_CHANGE)
{
  /* HOST->ZW: mode | funcID */
  if (ZW_NodeManagementRunning())
  {
    // A previous node management request is still in progress. Drop this request and go back to idle state.
    set_state_and_notify(stateIdle);
    return;
  }
  SetupNodeManagement(frame, 1);
  ControllerChange(frame->payload[0],
                   (funcID_ComplHandler_ZW_NodeManagement != 0) ? &ZCB_ComplHandler_ZW_NodeManagement : NULL);
}
#endif


#if SUPPORT_ZW_SET_LEARN_MODE
#ifdef ZW_SLAVE
uint8_t funcID_ComplHandler_ZW_SetLearnMode;

/*=========================   ComplHandler_ZW_SetLearnMode   =================
**    Completion handler for ZW_SetLearnMode
**
**--------------------------------------------------------------------------*/
static void                             /*RET Nothing                       */
ZCB_ComplHandler_ZW_SetLearnMode(
  uint32_t bStatus)                         /*IN  ZW_SetLearnMode status        */
{
  uint8_t i = 0;
  node_id_t node_id;

  node_id = ZAF_GetNodeID();
  BYTE_IN_AR(compl_workbuf, i++) = funcID_ComplHandler_ZW_SetLearnMode;
  BYTE_IN_AR(compl_workbuf, i++) = (uint8_t)bStatus;
  if (SERIAL_API_SETUP_NODEID_BASE_TYPE_16_BIT == nodeIdBaseType)
  {
    BYTE_IN_AR(compl_workbuf, i++) = (uint8_t)(node_id >> 8); // MSB 16bit node Id
  }
  BYTE_IN_AR(compl_workbuf, i++) = (uint8_t)(node_id & 0xFF); // LSB(16bit)/Legacy 8 bit node Id
  /* For safty we transmit len = 0, to indicate that no data follows */
  BYTE_IN_AR(compl_workbuf, i++) = 0;
  Request(FUNC_ID_ZW_SET_LEARN_MODE, compl_workbuf, i);
}
#endif /* ZW_SLAVE */

#ifdef ZW_CONTROLLER
static void ZCB_ZW_NodeManagementLearnStatusRelay(uint32_t Status)
{
  node_id_t node_id;

  node_id = ZAF_GetNodeID();

  LEARN_INFO_T Info = {
      .bStatus = (uint8_t)Status,
      .bSource = node_id,
      .pCmd = NULL,
      .bLen = 0
  };

  ZCB_ComplHandler_ZW_NodeManagement(&Info);
}
#endif /* ZW_CONTROLLER */

static uint8_t NetworkLearnModeStart(  E_NETWORK_LEARN_MODE_ACTION  eLearnMode)
{
  SZwaveCommandPackage learnModeStart = {
    .eCommandType = EZWAVECOMMANDTYPE_NETWORK_LEARN_MODE_START,
    .uCommandParams.SetSmartStartLearnMode.eLearnMode = eLearnMode};
  // Put the Command on queue (and dont wait for it, queue must be empty)
  if (EQUEUENOTIFYING_STATUS_SUCCESS == QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&learnModeStart, 0))
  {
    // Wait for protocol to handle command
    SZwaveCommandStatusPackage result = { 0 };
    if (GetCommandResponse(&result, EZWAVECOMMANDSTATUS_NETWORK_LEARN_MODE_START))
    {
      return result.Content.NetworkManagementStatus.statusInfo[0];
    }
  }
  return false;
}

ZW_ADD_CMD(FUNC_ID_ZW_SET_LEARN_MODE)
{
  /* HOST->ZW: mode | funcID */
  /* ZW->HOST: retVal */
  uint8_t retVal = 1;
#ifdef ZW_CONTROLLER
  SetupNodeManagement(frame, 1);
#endif /* ZW_CONTROLLER */
#ifdef ZW_SLAVE
  funcID_ComplHandler_ZW_SetLearnMode = frame->payload[1];
#endif
  SyncEventArg1Unbind(&LearnModeStatusCb);
  if (frame->payload[1] != 0)
  {
#ifdef ZW_CONTROLLER
    SyncEventArg1Bind(&LearnModeStatusCb, ZCB_ZW_NodeManagementLearnStatusRelay);
#endif
#ifdef ZW_SLAVE
    SyncEventArg1Bind(&LearnModeStatusCb, ZCB_ComplHandler_ZW_SetLearnMode);
#endif
  }

  if (SERIALPI_SET_LEARN_MODE_LEARN_PLUS_OFFSET > frame->payload[0])
  {
    /* Plain ZW_SetLearnMode */
    /* ZW_SET_LEARN_MODE_DISABLE          0x00 */
    /* ZW_SET_LEARN_MODE_CLASSIC          0x01 */
    /* ZW_SET_LEARN_MODE_NWI              0x02 */
    /* ZW_SET_LEARN_MODE_NWE              0x03 */
    SZwaveCommandPackage Command = {
      .eCommandType = EZWAVECOMMANDTYPE_SET_LEARN_MODE,
      .uCommandParams.SetLearnMode.eLearnMode = frame->payload[0],
      .uCommandParams.SetLearnMode.useCB = frame->payload[1] != 0
    };
    QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&Command, 0);
  }
  else
  {
    /* HOST want to use Network Management for inclusion/exclusion */
    /* We need to substract the SERIALPI_SET_LEARN_MODE_LEARN_PLUS_OFFSET to get: */
    /* E_NETWORK_LEARN_MODE_DISABLE =  0,      Disable learn process */
    /* E_NETWORK_LEARN_MODE_INCLUSION  = 1,    Enable the learn process to do an inclusion */
    /* E_NETWORK_LEARN_MODE_EXCLUSION  = 2,    Enable the learn process to do an exclusion */
    /* E_NETWORK_LEARN_MODE_EXCLUSION_NWE = 3  Enable the learn process to do a network wide exclusion */
    /* E_NETWORK_LEARN_MODE_INCLUSION_SMARTSTART = 4 Enable the learn process to initiate SMARTSTART inclusion */
    retVal = NetworkLearnModeStart(frame->payload[0] - SERIALPI_SET_LEARN_MODE_LEARN_PLUS_OFFSET);
  }
  DoRespond(retVal);
}
#endif /* SUPPORT_ZW_SET_LEARN_MODE */


#if SUPPORT_ZW_EXPLORE_REQUEST_INCLUSION
static uint8_t ExploreRequestInclusion(void)
{
  // Create transmit frame package
  SZwaveTransmitPackage FramePackage = {
    .eTransmitType = EZWAVETRANSMITTYPE_EXPLOREINCLUSIONREQUEST
  };

  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_EXPLORE_REQUEST_INCLUSION)
{
  const uint8_t retVal = ExploreRequestInclusion();
  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_EXPLORE_REQUEST_EXCLUSION
static uint8_t ExploreRequestExclusion(void)
{
  // Create transmit frame package
  SZwaveTransmitPackage FramePackage = {
    .eTransmitType = EZWAVETRANSMITTYPE_EXPLOREEXCLUSIONREQUEST
  };

  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_EXPLORE_REQUEST_EXCLUSION)
{
  const uint8_t retVal = ExploreRequestExclusion();
  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_REPLICATION_COMMAND_COMPLETE
static void ReplicationComplete(void)
{
  /*  */
  // Create transmit frame package
  SZwaveTransmitPackage FramePackage = {
    .eTransmitType = EZWAVETRANSMITTYPE_SENDREPLICATIONRECEIVECOMPLETE
  };
  // Put the package on queue (and dont wait for it)
  QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
}

ZW_ADD_CMD(FUNC_ID_ZW_REPLICATION_COMMAND_COMPLETE)
{
  ReplicationComplete();
  set_state_and_notify(stateIdle);
}
#endif


#if SUPPORT_ZW_REPLICATION_SEND_DATA
uint8_t funcID_ComplHandler_ZW_ReplicationSendData;

/*=====================   ComplHandler_ZW_ReplicationSendData   =============
**    Completion handler for ZW_ReplicationSendData
**
**--------------------------------------------------------------------------*/
static void /* RET  Nothing */
ZCB_ComplHandler_ZW_ReplicationSendData(
    uint8_t txStatus, __attribute__((unused)) TX_STATUS_TYPE* extendedTxStatus) /* IN   Transmit completion status */
{
  BYTE_IN_AR(compl_workbuf, 0) = funcID_ComplHandler_ZW_ReplicationSendData;
  BYTE_IN_AR(compl_workbuf, 1) = txStatus;
  Request(FUNC_ID_ZW_REPLICATION_SEND_DATA, compl_workbuf, 2);
}

static uint8_t ReplicationSendData(uint16_t nodeID, uint8_t dataLength, const uint8_t* pData, uint8_t txOptions, ZW_TX_Callback_t pCallBack)
{
  assert(dataLength <= BUF_SIZE_RX);
  dataLength = MIN(dataLength, BUF_SIZE_RX);
  SZwaveTransmitPackage FramePackage = {
    .uTransmitParams.SendReplication.DestNodeId = nodeID,
    .uTransmitParams.SendReplication.FrameConfig.TransmitOptions = txOptions,
    .uTransmitParams.SendReplication.FrameConfig.Handle = pCallBack,
    .eTransmitType = EZWAVETRANSMITTYPE_SENDREPLICATION,
    .uTransmitParams.SendReplication.FrameConfig.iFrameLength = dataLength
  };
  memcpy(&FramePackage.uTransmitParams.SendReplication.FrameConfig.aFrame, pData, dataLength);

  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_REPLICATION_SEND_DATA)
{
  /* nodeID | dataLength | pData[] | txOptions | funcID */
  uint8_t dataLength;
  uint8_t txOptions;
  uint8_t offset = 0;
  node_id_t nodeID;
  nodeID = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  dataLength = frame->payload[offset + 1];
  txOptions = frame->payload[offset + 2 + dataLength];
  funcID_ComplHandler_ZW_ReplicationSendData = frame->payload[offset + 3 + dataLength];
  const uint8_t retVal = ReplicationSendData(nodeID, dataLength, &frame->payload[offset + 2], txOptions,
                                (funcID_ComplHandler_ZW_ReplicationSendData != 0) ? &ZCB_ComplHandler_ZW_ReplicationSendData : NULL );

  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_ASSIGN_RETURN_ROUTE
uint8_t funcID_ComplHandler_ZW_AssignReturnRoute;

/*=====================   ComplHandler_ZW_AssignReturnRoute   =============
**    Completion handler for ZW_AssignReturnRoute
**
**--------------------------------------------------------------------------*/
static void /* RET  Nothing                     */
ZCB_ComplHandler_ZW_AssignReturnRoute(
    uint8_t bStatus,
    TX_STATUS_TYPE *txStatusReport) /* IN   Transmit completion status  */
{
  uint8_t bIdx = 0;
  BYTE_IN_AR(compl_workbuf, bIdx++) = funcID_ComplHandler_ZW_AssignReturnRoute;
  BYTE_IN_AR(compl_workbuf, bIdx++) = bStatus;
  if (bTxStatusReportEnabled && txStatusReport) /* Check if detailed info is available from protocol */
  {
    memcpy(&compl_workbuf[bIdx], (uint8_t *)txStatusReport, sizeof(TX_STATUS_TYPE));
    bIdx += sizeof(TX_STATUS_TYPE);
  }
  Request(FUNC_ID_ZW_ASSIGN_RETURN_ROUTE, compl_workbuf, bIdx);
}

static uint8_t AssignReturnRoute(uint16_t srcNode, uint16_t destNode,  ZW_TX_Callback_t pCallBack)
{
 // Create transmit frame package
  SZwaveTransmitPackage FramePackage = {
    .uTransmitParams.AssignReturnRoute.ReturnRouteReceiverNodeId = srcNode,
    .uTransmitParams.AssignReturnRoute.RouteDestinationNodeId = destNode,
    .uTransmitParams.AssignReturnRoute.Handle  = (ZW_Void_Callback_t)pCallBack,
    .eTransmitType = EZWAVETRANSMITTYPE_ASSIGNRETURNROUTE
  };
  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_ASSIGN_RETURN_ROUTE)
{
  /* srcNodeID | destNodeID | funcID */
  uint8_t offset = 0;
  node_id_t srcNodeID;
  node_id_t destNodeID;
  srcNodeID  = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  destNodeID = (node_id_t)GET_NODEID(&frame->payload[1 + offset], offset);
  funcID_ComplHandler_ZW_AssignReturnRoute = frame->payload[2 + offset];
  const uint8_t retVal = AssignReturnRoute(srcNodeID, destNodeID,
                              (funcID_ComplHandler_ZW_AssignReturnRoute != 0) ? &ZCB_ComplHandler_ZW_AssignReturnRoute : NULL);

  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_ASSIGN_PRIORITY_RETURN_ROUTE
uint8_t funcID_ComplHandler_ZW_AssignPriorityReturnRoute;

/*===============   ZCB_ComplHandler_ZW_AssignPriorityReturnRoute   =========
**    Completion handler for ZW_AssignPriorityReturnRoute
**
**--------------------------------------------------------------------------*/
static void /* RET  Nothing                     */
ZCB_ComplHandler_ZW_AssignPriorityReturnRoute(
    uint8_t bStatus, /* IN   Transmit completion status  */
    TX_STATUS_TYPE *txStatusReport)
{
  uint8_t bIdx = 0;
  BYTE_IN_AR(compl_workbuf, bIdx++) = funcID_ComplHandler_ZW_AssignPriorityReturnRoute;
  BYTE_IN_AR(compl_workbuf, bIdx++) = bStatus;
  if (bTxStatusReportEnabled && txStatusReport) /* Check if detailed info is available from protocol */
  {
    memcpy(&compl_workbuf[bIdx], (uint8_t *)txStatusReport, sizeof(TX_STATUS_TYPE));
    bIdx += sizeof(TX_STATUS_TYPE);
  }
  Request(FUNC_ID_ZW_ASSIGN_PRIORITY_RETURN_ROUTE, compl_workbuf, bIdx);
}

static uint8_t AssignPriorityReturnRoute(uint16_t srcNode, uint16_t destNode, const uint8_t* pRoute, uint8_t routeSpeed,  ZW_TX_Callback_t pCallBack)
{
  /* srcNodeID | destNodeID | route[5] | funcID */
  // Create transmit frame package
  SZwaveTransmitPackage FramePackage = {
    .uTransmitParams.AssignReturnRoute.ReturnRouteReceiverNodeId = srcNode,
    .uTransmitParams.AssignReturnRoute.RouteDestinationNodeId = destNode,
    .uTransmitParams.AssignReturnRoute.PriorityRouteSpeed = routeSpeed,
    .uTransmitParams.AssignReturnRoute.Handle = (ZW_Void_Callback_t)pCallBack,
    .eTransmitType = EZWAVETRANSMITTYPE_ASSIGNRETURNROUTE
  };
  memcpy(FramePackage.uTransmitParams.AssignReturnRoute.aPriorityRouteRepeaters, pRoute,
          sizeof(FramePackage.uTransmitParams.AssignReturnRoute.aPriorityRouteRepeaters));
  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_ASSIGN_PRIORITY_RETURN_ROUTE)
{
  /* srcNodeID | destNodeID | route[5] | funcID */
  uint8_t  offset = 0;
  node_id_t srcNodeID;
  node_id_t  destNodeID;
  srcNodeID  = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  destNodeID = (node_id_t)GET_NODEID(&frame->payload[1 + offset], offset);
  funcID_ComplHandler_ZW_AssignPriorityReturnRoute = frame->payload[offset + 7];
  // Put the package on queue (and dont wait for it)
  const uint8_t retVal = AssignPriorityReturnRoute(srcNodeID, destNodeID, &frame->payload[offset + 2], frame->payload[offset + 6],
                                      (funcID_ComplHandler_ZW_AssignPriorityReturnRoute != 0) ? &ZCB_ComplHandler_ZW_AssignPriorityReturnRoute : NULL );

  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_DELETE_RETURN_ROUTE
uint8_t funcID_ComplHandler_ZW_DeleteReturnRoute;

/*=====================   ComplHandler_ZW_DeleteReturnRoute   =============
**    Completion handler for ZW_DeleteReturnRoute
**
**--------------------------------------------------------------------------*/
static void /* RET  Nothing                     */
ZCB_ComplHandler_ZW_DeleteReturnRoute(
    uint8_t bStatus,
    TX_STATUS_TYPE *txStatusReport) /* IN   Transmit completion status  */
{
  uint8_t bIdx = 0;
  BYTE_IN_AR(compl_workbuf, bIdx++) = funcID_ComplHandler_ZW_DeleteReturnRoute;
  BYTE_IN_AR(compl_workbuf, bIdx++) = bStatus;
  if (bTxStatusReportEnabled /* Do HOST want txStatusReport */
      && txStatusReport)     /* Check if detailed info is available from protocol */
  {
    memcpy(&compl_workbuf[bIdx], (uint8_t *)txStatusReport, sizeof(TX_STATUS_TYPE));
    bIdx += sizeof(TX_STATUS_TYPE);
  }
  Request(FUNC_ID_ZW_DELETE_RETURN_ROUTE, compl_workbuf, bIdx);
}

static uint8_t DeleteReturnNode(uint16_t nodeID, ZW_TX_Callback_t pCallBack)
{
  // Create transmit frame package
  SZwaveTransmitPackage FramePackage = {
    .uTransmitParams.DeleteReturnRoute.DestNodeId = nodeID,
    .uTransmitParams.DeleteReturnRoute.Handle = (ZW_Void_Callback_t)pCallBack,
    .eTransmitType = EZWAVETRANSMITTYPE_DELETERETURNROUTE
  };
  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_DELETE_RETURN_ROUTE)
{
  /* nodeID | funcID */
  uint8_t offset = 0;
  node_id_t nodeId = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  funcID_ComplHandler_ZW_DeleteReturnRoute = frame->payload[1 + offset];
  const uint8_t retVal = DeleteReturnNode(nodeId, (funcID_ComplHandler_ZW_DeleteReturnRoute != 0) ? &ZCB_ComplHandler_ZW_DeleteReturnRoute : NULL);
  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_ASSIGN_SUC_RETURN_ROUTE
static uint8_t AssignSucReturnRoute(uint16_t srcNodeID, uint8_t sucNode, ZW_TX_Callback_t pCallBack)
{
  /* srcNodeID | funcID */
  // Create transmit frame package
  SZwaveTransmitPackage FramePackage = {
    .uTransmitParams.AssignReturnRoute.ReturnRouteReceiverNodeId = srcNodeID,
    .uTransmitParams.AssignReturnRoute.RouteDestinationNodeId = sucNode,
    .uTransmitParams.AssignReturnRoute.isSucRoute = true,
    .uTransmitParams.AssignReturnRoute.Handle = (ZW_Void_Callback_t)pCallBack,
    .eTransmitType = EZWAVETRANSMITTYPE_ASSIGNRETURNROUTE
  };
  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_ASSIGN_SUC_RETURN_ROUTE)
{
  /* srcNodeID | funcID */
  uint8_t retVal = 0;
  uint8_t SUCNodeId = (uint8_t)(ZAF_GetSucNodeId() & 0xFF);
  uint8_t offset = 0;
  node_id_t srcNodeID = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  if (SUCNodeId != 0)
  {
    funcID_ComplHandler_netWork_Management = frame->payload[1 + offset];
    retVal = AssignSucReturnRoute(srcNodeID, SUCNodeId, (funcID_ComplHandler_netWork_Management != 0) ? &ZCB_ComplHandler_ZW_netWork_Management : NULL);
    management_Func_ID = frame->cmd;
  }
  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_ASSIGN_PRIORITY_SUC_RETURN_ROUTE
uint8_t funcID_ComplHandler_ZW_AssignPrioritySUCReturnRoute;

static uint8_t AssignPrioritySucReturnRoute(uint16_t srcNode, uint8_t sucNode,  const uint8_t* pRoute, uint8_t routeSpeed, ZW_TX_Callback_t pCallBack)
{
 // Create transmit frame package
  SZwaveTransmitPackage FramePackage = {
    .uTransmitParams.AssignReturnRoute.ReturnRouteReceiverNodeId = srcNode,
    .uTransmitParams.AssignReturnRoute.RouteDestinationNodeId = sucNode,
    .uTransmitParams.AssignReturnRoute.PriorityRouteSpeed = routeSpeed,
    .uTransmitParams.AssignReturnRoute.isSucRoute = true,
    .uTransmitParams.AssignReturnRoute.Handle = (ZW_Void_Callback_t)pCallBack,
    .eTransmitType = EZWAVETRANSMITTYPE_ASSIGNRETURNROUTE
  };
  memcpy(FramePackage.uTransmitParams.AssignReturnRoute.aPriorityRouteRepeaters, pRoute, sizeof(FramePackage.uTransmitParams.AssignReturnRoute.aPriorityRouteRepeaters));
  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_ASSIGN_PRIORITY_SUC_RETURN_ROUTE)
{
  /* srcNodeID | route[5] | funcID */
  uint8_t retVal = 0;
  uint8_t SUCNodeId = (uint8_t)(ZAF_GetSucNodeId() & 0xFF);
  uint8_t offset = 0;
  node_id_t srcNodeID = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  if (SUCNodeId != 0)
  {
    funcID_ComplHandler_netWork_Management = frame->payload[offset + 6];
    retVal = AssignPrioritySucReturnRoute(srcNodeID, SUCNodeId, &frame->payload[offset + 1],  frame->payload[offset + 1 + 4],
                                            (funcID_ComplHandler_netWork_Management != 0) ? &ZCB_ComplHandler_ZW_netWork_Management : NULL);

    management_Func_ID = frame->cmd;
  }
  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_DELETE_SUC_RETURN_ROUTE
static uint8_t DeleteSucReturnRoute(uint16_t srcNode, ZW_TX_Callback_t pCallBack)
{
  /* srcNodeID | funcID */

  // Create transmit frame package
  SZwaveTransmitPackage FramePackage = {
    .uTransmitParams.DeleteReturnRoute.DestNodeId = srcNode,
    .uTransmitParams.DeleteReturnRoute.bDeleteSuc = true,
    .uTransmitParams.DeleteReturnRoute.Handle = (ZW_Void_Callback_t)pCallBack,
    .eTransmitType = EZWAVETRANSMITTYPE_DELETERETURNROUTE
  };
  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_DELETE_SUC_RETURN_ROUTE)
{
  /* srcNodeID | funcID */
  uint8_t offset = 0;
  node_id_t nodeId = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  funcID_ComplHandler_netWork_Management = frame->payload[1 + offset];
  const uint8_t retVal = DeleteSucReturnRoute(nodeId,
                                (funcID_ComplHandler_netWork_Management != 0) ? &ZCB_ComplHandler_ZW_netWork_Management : NULL);
  management_Func_ID = frame->cmd;
  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_SEND_SUC_ID
uint8_t funcID_ComplHandler_ZW_SendSUC_ID;

static void
ZCB_ComplHandler_ZW_SendSUC_ID(
    uint8_t bStatus,
    __attribute__((unused)) TX_STATUS_TYPE *txStatusReport)
{
  BYTE_IN_AR(compl_workbuf, 0) = funcID_ComplHandler_ZW_SendSUC_ID;
  BYTE_IN_AR(compl_workbuf, 1) = bStatus;
  Request(FUNC_ID_ZW_SEND_SUC_ID, compl_workbuf, 2);
}

static uint8_t SendSucID(uint16_t destNode, uint8_t txOptions, ZW_TX_Callback_t pCallBack)
{
  // Create transmit frame package
  SZwaveTransmitPackage FramePackage = {
    .uTransmitParams.SendSucNodeId.DestNodeId = destNode,
    .uTransmitParams.SendSucNodeId.TransmitOptions = txOptions,
    .uTransmitParams.SendSucNodeId.Handle = (ZW_Void_Callback_t)pCallBack,
    .eTransmitType = EZWAVETRANSMITTYPE_SENDSUCNODEID
  };
  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_SEND_SUC_ID)
{
  /* destNodeID | txOptions | funcID */
  uint8_t  offset = 0;
  node_id_t destNodeID = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  funcID_ComplHandler_ZW_SendSUC_ID = frame->payload[offset + 2];

  const uint8_t retVal = SendSucID(destNodeID, frame->payload[offset + 2],
                      (funcID_ComplHandler_ZW_SendSUC_ID != 0) ? &ZCB_ComplHandler_ZW_SendSUC_ID : NULL);
  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_SET_SUC_NODE_ID
uint8_t funcID_ComplHandler_ZW_SetSUCNodeID;

/*====================   ComplHandler_ZW_SetSUCNodeID   ======================
**    Function description
**    ZW_SUC_SET_SUCCEEDED
**    Side effects:
**
**--------------------------------------------------------------------------*/
static void
ZCB_ComplHandler_ZW_SetSUCNodeID(
    uint8_t txStatus, /*IN   Completion status*/
    __attribute__((unused)) TX_STATUS_TYPE *txStatusReport)
{
  BYTE_IN_AR(compl_workbuf, 0) = funcID_ComplHandler_ZW_SetSUCNodeID;
  BYTE_IN_AR(compl_workbuf, 1) = txStatus;
  Request(FUNC_ID_ZW_SET_SUC_NODE_ID, compl_workbuf, 2);
}

static uint8_t SetSucNodeID(uint16_t nodeID, uint8_t sucState, uint8_t txOptions, uint8_t capabilities, ZW_TX_Callback_t pCallBack)
{
  // Create transmit frame package
  SZwaveTransmitPackage FramePackage = {
    .uTransmitParams.SetSucNodeId.SucNodeId = nodeID,
    .uTransmitParams.SetSucNodeId.bSucEnable = sucState,
    .uTransmitParams.SetSucNodeId.bTxLowPower = txOptions,
    .uTransmitParams.SetSucNodeId.Capabilities = capabilities,
    .uTransmitParams.SetSucNodeId.Handle = (ZW_Void_Callback_t)pCallBack,
    .eTransmitType = EZWAVETRANSMITTYPE_SETSUCNODEID
  };
  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_SET_SUC_NODE_ID)
{
  /* nodeID | SUCState | txOptions | capabilities | funcID */
  uint8_t offset = 0;
  node_id_t nodeId = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  funcID_ComplHandler_ZW_SetSUCNodeID = frame->payload[offset + 4];
  const uint8_t retVal = SetSucNodeID(nodeId, frame->payload[offset + 1], frame->payload[offset + 2], frame->payload[offset + 3],
                        (funcID_ComplHandler_ZW_SetSUCNodeID != 0) ? &ZCB_ComplHandler_ZW_SetSUCNodeID : NULL);
  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_GET_SUC_NODE_ID
ZW_ADD_CMD(FUNC_ID_ZW_GET_SUC_NODE_ID)
{
  uint8_t cmdLength = 0;
  node_id_t suc_node_id;

  suc_node_id = ZAF_GetSucNodeId();

  if (SERIAL_API_SETUP_NODEID_BASE_TYPE_16_BIT == nodeIdBaseType)
  {
    BYTE_IN_AR(compl_workbuf, 0) = (uint8_t)(suc_node_id >> 8);      // MSB
    BYTE_IN_AR(compl_workbuf, 1) = (uint8_t)(suc_node_id & 0xFF);    // LSB
    cmdLength = 2;
  }
  else
  {
    BYTE_IN_AR(compl_workbuf, 0) = (uint8_t)(suc_node_id & 0xFF);
    cmdLength = 1;
  }
  DoRespond_workbuf(cmdLength);
}
#endif


#if SUPPORT_ZW_REMOVE_FAILED_NODE_ID
uint8_t funcID_ComplHandler_ZW_RemoveFailedNodeID;

/*=====================   ComplHandler_ZW_RemoveFailedNodeID   ==============
**    Completion handler for ZW_RemoveFailedNodeID
**
**--------------------------------------------------------------------------*/
void /* RET  Nothing                     */
ZCB_ComplHandler_ZW_RemoveFailedNodeID(
    uint8_t bStatus)
{
  if (0 == funcID_ComplHandler_ZW_RemoveFailedNodeID)
  {
    return;
  }

  BYTE_IN_AR(compl_workbuf, 0) = funcID_ComplHandler_ZW_RemoveFailedNodeID;
  BYTE_IN_AR(compl_workbuf, 1) = bStatus;
  Request(FUNC_ID_ZW_REMOVE_FAILED_NODE_ID, compl_workbuf, 2);
}

static uint8_t RemoveFailedNode(uint16_t nodeID)
{

  SZwaveCommandPackage cmdPackage = {
    .eCommandType = EZWAVECOMMANDTYPE_REMOVE_FAILED_NODE_ID,
    .uCommandParams.FailedNodeIDCmd.nodeID = nodeID
  };
  // Put the package on queue (and dont wait for it)
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&cmdPackage, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
  SZwaveCommandStatusPackage cmdStatus = { 0 };
  if (GetCommandResponse(&cmdStatus, EZWAVECOMMANDSTATUS_REMOVE_FAILED_NODE_ID))
  {
    return cmdStatus.Content.FailedNodeIDStatus.result;
  }
  assert(0);
  return 0;
}

ZW_ADD_CMD(FUNC_ID_ZW_REMOVE_FAILED_NODE_ID)
{
  /* nodeID | funcID */
  uint8_t offset = 0;
  node_id_t nodeId = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  funcID_ComplHandler_ZW_RemoveFailedNodeID = frame->payload[1 + offset];
  const uint8_t retVal = RemoveFailedNode(nodeId);
  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_IS_FAILED_NODE_ID
static uint8_t IsNodeIDFailed(uint16_t nodeID)
{
  SZwaveCommandPackage cmdPackage = {
      .eCommandType = EZWAVECOMMANDTYPE_IS_FAILED_NODE_ID,
      .uCommandParams.IsFailedNodeID.nodeID = nodeID};

  // Put the Command on queue (and dont wait for it, queue must be empty)
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&cmdPackage, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);

  // Wait for protocol to handle command (it shouldnt take long)
  SZwaveCommandStatusPackage cmdStatus = { 0 };
  if (GetCommandResponse(&cmdStatus, EZWAVECOMMANDSTATUS_IS_FAILED_NODE_ID))
  {
    return cmdStatus.Content.IsFailedNodeIDStatus.result;
  }
  assert(false); // FIXME We should have more intelligent error handling, we shouldnt assert here.
  return 0;
}

ZW_ADD_CMD(FUNC_ID_ZW_IS_FAILED_NODE_ID)
{
  /* nodeID */
  volatile uint8_t offset = 0;
  node_id_t nodeId = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  const uint8_t retVal = IsNodeIDFailed(nodeId);
  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_REPLACE_FAILED_NODE
uint8_t funcID_ComplHandler_ZW_ReplaceFailedNode;

/*=====================   ComplHandler_ZW_RemoveFailedNodeID   ==============
**    Completion handler for ZW_RemoveFailedNodeID
**
**--------------------------------------------------------------------------*/
void /* RET  Nothing                     */
ZCB_ComplHandler_ZW_ReplaceFailedNode(
    uint8_t bStatus) /* IN   Transmit completion status  */
{
  if (0 == funcID_ComplHandler_ZW_ReplaceFailedNode)
  {
    return;
  }

  BYTE_IN_AR(compl_workbuf, 0) = funcID_ComplHandler_ZW_ReplaceFailedNode;
  BYTE_IN_AR(compl_workbuf, 1) = bStatus;
  Request(FUNC_ID_ZW_REPLACE_FAILED_NODE, compl_workbuf, 2);
}

static uint8_t ReplaceFailedNode(uint16_t nodeID, uint8_t normalPower)
{
  SZwaveCommandPackage cmdPackage = {
    .eCommandType = EZWAVECOMMANDTYPE_REPLACE_FAILED_NODE_ID,
    .uCommandParams.FailedNodeIDCmd.nodeID = nodeID,
    .uCommandParams.FailedNodeIDCmd.normalPower = normalPower
  };
  // Put the package on queue (and dont wait for it)
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&cmdPackage, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
  SZwaveCommandStatusPackage cmdStatus = { 0 };
  if (GetCommandResponse(&cmdStatus, EZWAVECOMMANDSTATUS_REPLACE_FAILED_NODE_ID))
  {
    return cmdStatus.Content.FailedNodeIDStatus.result;
  }
  assert(0);
  return 0;

}

ZW_ADD_CMD(FUNC_ID_ZW_REPLACE_FAILED_NODE)
{
  /* nodeID | funcID */
  uint8_t offset = 0;
  node_id_t nodeId = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  funcID_ComplHandler_ZW_ReplaceFailedNode = frame->payload[1 + offset];
  const uint8_t retVal = ReplaceFailedNode(nodeId, true);
  DoRespond(retVal);
}
#endif


#if SUPPORT_GET_ROUTING_TABLE_LINE
static void GetRoutingInfo(uint16_t nodeID, uint8_t options, uint8_t *pRoutingInfo)
{
  SZwaveCommandPackage cmdPackage = {
      .eCommandType = EZWAVECOMMANDTYPE_GET_ROUTING_TABLE_LINE,
      .uCommandParams.GetRoutingInfo.nodeID = nodeID,
      .uCommandParams.GetRoutingInfo.options = options};
  // Put the Command on queue (and dont wait for it, queue must be empty)
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&cmdPackage, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);

  // Wait for protocol to handle command (it shouldnt take long)
  SZwaveCommandStatusPackage cmdStatus = { 0 };
  if (GetCommandResponse(&cmdStatus, EZWAVECOMMANDSTATUS_GET_ROUTING_TABLE_LINE))
  {
    memcpy(pRoutingInfo, cmdStatus.Content.GetRoutingInfoStatus.RoutingInfo, MAX_NODEMASK_LENGTH);
    return;
  }
  assert(false); // FIXME We should have more intelligent error handling, we shouldnt assert here.
}

ZW_ADD_CMD(FUNC_ID_GET_ROUTING_TABLE_LINE)
{
  /* HOST->ZW: bLine | bRemoveBad | bRemoveNonReps */
  uint8_t offset = 0;
  node_id_t nodeId = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  GetRoutingInfo(nodeId,
                  (uint8_t)(((frame->payload[offset + 1]) ? GET_ROUTING_INFO_REMOVE_BAD : 0) |
                            ((frame->payload[offset + 2]) ? GET_ROUTING_INFO_REMOVE_NON_REPS : 0)),
                  compl_workbuf);
  DoRespond_workbuf(MAX_NODEMASK_LENGTH);
}
#endif


#if SUPPORT_GET_TX_COUNTER
ZW_ADD_CMD(FUNC_ID_GET_TX_COUNTER)
{
  /* */
  /* Get the transmit counter */
  /* Not usedin 700 series - Obsoleted */
  DoRespond(0);
}
#endif


#if SUPPORT_RESET_TX_COUNTER
ZW_ADD_CMD(FUNC_ID_RESET_TX_COUNTER)
{
  /* */
  /* Reset the transmit counter */
  /* Not usedin 700 series - Obsoleted */
  DoRespond(0);
}
#endif


#if SUPPORT_STORE_HOMEID
static void StoreHomeID(uin8_t *pHomeID, uint16_t nodeID)
{
  SZwaveCommandPackage cmdPackage = {
      .eCommandType = EZWAVECOMMANDTYPE_STORE_HOMEID,
      .uCommandParams.StoreHomeID.nodeID = nodeID,
  };
  memcpy(cmdPackage.uCommandParams.StoreHomeID.homeID, pHomeID, sizeof(cmdPackage.uCommandParams.StoreHomeID.homeID));
  // Put the Command on queue (and dont wait for it, queue must be empty)
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&cmdPackage, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
}

ZW_ADD_CMD(FUNC_ID_STORE_HOMEID)
{
  /* homeID1 | homeID2 | homeID3 | homeID4 | nodeID */
  /* Store homeID and Node ID. */
  uint8_t offset = 0;
  StoreHomeID(frame->payload, GET_NODEID(&frame->payload[4], offset));
  set_state_and_notify(stateIdle);
}
#endif


#if SUPPORT_LOCK_ROUTE_RESPONSE
static void LockResponseRoute(uint8_t lockID)
{
  SZwaveCommandPackage cmdPackage = {
      .eCommandType = EZWAVECOMMANDTYPE_LOCK_ROUTE_RESPONSE,
      .uCommandParams.LockRouteResponse.value = lockID,
  };
  // Put the Command on queue (and dont wait for it, queue must be empty)
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&cmdPackage, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
}

ZW_ADD_CMD(FUNC_ID_LOCK_ROUTE_RESPONSE)
{
  /* HOST->ZW: lockID */
  /* Lock response routes / Last Working Routes. lockID == nodeID for locking. lockID == 0x00 to unlock */
  LockResponseRoute(frame->payload[0]);
  set_state_and_notify(stateIdle);
}
#endif


#if SUPPORT_ZW_GET_PRIORITY_ROUTE
static uint8_t GetPriorityRoute(uint16_t nodeID, uint8_t *priRoute)
{
  SZwaveCommandPackage cmdPackage = {
      .eCommandType = EZWAVECOMMANDTYPE_GET_PRIORITY_ROUTE,
      .uCommandParams.GetPriorityRoute.nodeID = nodeID,
      .uCommandParams.GetPriorityRoute.pPriRouteBuffer = priRoute,
  };
  // Put the Command on queue (and dont wait for it, queue must be empty)
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&cmdPackage, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);

  // Wait for protocol to handle command (it shouldnt take long)
  SZwaveCommandStatusPackage cmdStatus = { 0 };
  if (GetCommandResponse(&cmdStatus, EZWAVECOMMANDSTATUS_GET_PRIORITY_ROUTE))
  {
    memcpy(priRoute, cmdStatus.Content.GetPriorityRouteStatus.repeaters, MAX_REPEATERS);
    priRoute[4] = cmdStatus.Content.GetPriorityRouteStatus.routeSpeed;
    return cmdStatus.Content.GetPriorityRouteStatus.bAnyRouteFound;
  }
  assert(false); // FIXME We should have more intelligent error handling, we shouldnt assert here.
  return 0;
}

ZW_ADD_CMD(FUNC_ID_ZW_GET_PRIORITY_ROUTE)
{
  /* HOST->ZW: nodeID */
  /* ZW->HOST: nodeID | anyRouteFound | repeater0 | repeater1 | repeater2 | repeater3 | routespeed */
  uint8_t  offset = 0;
  node_id_t nodeId = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  if (SERIAL_API_SETUP_NODEID_BASE_TYPE_16_BIT == nodeIdBaseType)
  {
    BYTE_IN_AR(compl_workbuf, 0) = (uint8_t)(nodeId >> 8);     // MSB
    BYTE_IN_AR(compl_workbuf, 1) = (uint8_t)(nodeId & 0xFF);   // LSB
  }
  else
  {
    BYTE_IN_AR(compl_workbuf, 0) = (uint8_t)(nodeId & 0xFF);   // Legacy 8 bit nodeIDs
  }
  BYTE_IN_AR(compl_workbuf, offset + 1) = GetPriorityRoute(nodeId, &compl_workbuf[offset + 2]);
  DoRespond_workbuf(offset + 7);
}
#endif


#if SUPPORT_ZW_SET_PRIORITY_ROUTE
static uint8_t SetPriorityRoute(uint16_t nodeID, const uint8_t *routeInfo)
{
  SZwaveCommandPackage cmdPackage = {
      .eCommandType = EZWAVECOMMANDTYPE_SET_PRIORITY_ROUTE,
      .uCommandParams.SetPriorityRoute.nodeID = nodeID,
  };
  if (NULL != routeInfo)
  {
    memcpy(cmdPackage.uCommandParams.SetPriorityRoute.repeaters, routeInfo, MAX_REPEATERS);
    cmdPackage.uCommandParams.SetPriorityRoute.routeSpeed = routeInfo[4];
  }
  else
  {
    cmdPackage.uCommandParams.SetPriorityRoute.clearGolden = true;
  }
  // Put the Command on queue (and dont wait for it, queue must be empty)
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&cmdPackage, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);

  // Wait for protocol to handle command (it shouldnt take long)
  SZwaveCommandStatusPackage cmdStatus = { 0 };
  if (GetCommandResponse(&cmdStatus, EZWAVECOMMANDSTATUS_SET_PRIORITY_ROUTE))
  {
    return cmdStatus.Content.SetPriorityRouteStatus.bRouteUpdated;
  }
  assert(false); // FIXME We should have more intelligent error handling, we shouldnt assert here.
  return 0;
}

ZW_ADD_CMD(FUNC_ID_ZW_SET_PRIORITY_ROUTE)
{
  /* HOST->ZW: nodeID | repeater0 | repeater1 | repeater2 | repeater3 | routespeed */
  /* ZW->HOST: nodeID | routeUpdated */
  uint8_t  offset = 0;
  node_id_t nodeId = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  if (SERIAL_API_SETUP_NODEID_BASE_TYPE_16_BIT == nodeIdBaseType)
  {
    BYTE_IN_AR(compl_workbuf, 0) = (uint8_t)(nodeId >> 8);     // MSB
    BYTE_IN_AR(compl_workbuf, 1) = (uint8_t)(nodeId & 0xFF);   // LSB
  }
  else
  {
    BYTE_IN_AR(compl_workbuf, 0) = (uint8_t)(nodeId & 0xFF);   // Legacy 8 bit nodeIDs
  }
  if ((offset + 9) <= frame->len)
  {
    /* Set Priority Route Devkit 6.6x */
    BYTE_IN_AR(compl_workbuf, offset + 1) = SetPriorityRoute(nodeId, &frame->payload[offset + 1]);
  }
  else
  {
    /* Clear/Release Golden Route - Devkit 6.6x+ */
    BYTE_IN_AR(compl_workbuf, offset + 1) = SetPriorityRoute(nodeId, NULL);
  }
  DoRespond_workbuf(2);
}
#endif


#if SUPPORT_ZW_GET_VERSION
ZW_ADD_CMD(FUNC_ID_ZW_GET_VERSION)
{
  /* */
  const SProtocolInfo* protocol_info = ZAF_getProtocolInfo();
  uint8_t versionMinor = protocol_info->ProtocolVersion.Minor;
  if (255 == zpal_get_app_version_major())
  {
    // Special case when running the custom v255 file that is used for testing OTW firmware update.
    // Make ZW_GET_VERSION return a unique version string "Z-Wave 7.99" so that test tools can distinguish it from the normal builds.
    versionMinor = 99;
  }
  __attribute__((unused)) volatile int32_t iCharacters = snprintf((char *)(&compl_workbuf[0]), 12, "Z-Wave %1d.%02d", protocol_info->ProtocolVersion.Major, versionMinor);
  assert(iCharacters == 11); // Serial API must deliver 13 bytes reply. 11 byte string (no zero termination) followed by zero and 1 byte lib type
                              // We use SNPRINTF zero termination to produce the zero.
  _Static_assert(sizeof(compl_workbuf) >= 13, "STATIC_ASSERT_compl_workbuf_to_small");
  compl_workbuf[12] = protocol_info->eLibraryType;
  DoRespond_workbuf(13);
}
#endif


#if SUPPORT_ZW_GET_PROTOCOL_VERSION
ZW_ADD_CMD(FUNC_ID_ZW_GET_PROTOCOL_VERSION)
{
  /*  */
  uint8_t len;
  func_id_zw_get_protocol_version(0, NULL, compl_workbuf, &len);
  DoRespond_workbuf(len);
}
#endif


#if SUPPORT_SERIAL_API_APPL_NODE_INFORMATION
#ifdef ZW_CONTROLLER
static void ZW_UpdateCtrlNodeInformation_API_IF(void)
{
  // Create transmit frame package
  SZwaveCommandPackage FramePackage ={
    .eCommandType =  EZWAVECOMMANDTYPE_ZW_UPDATE_CTRL_NODE_INFORMATION,
    .uCommandParams.UpdateCtrlNodeInformation.value = true
  };
  // Put the package on queue (and dont wait for it)
  QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&FramePackage, 0);
}
#endif

ZW_ADD_CMD(FUNC_ID_SERIAL_API_APPL_NODE_INFORMATION)
{
  /* listening | generic | specific | parmLength | nodeParms[] */
  SAppNodeInfo_t *AppNodeInfo;

  AppNodeInfo = zaf_get_app_node_info();

  AppNodeInfo->DeviceOptionsMask = frame->payload[0];
  AppNodeInfo->NodeType.generic = frame->payload[1];
  AppNodeInfo->NodeType.specific = frame->payload[2];

  // As this serial API command only supports one set of command classes,
  // we use the same list for the entire CC set

  // Data for loopifying CC list writes
  SCommandClassList_t *const apCCLists[3] =
  {
    &CommandClasses.UnSecureIncludedCC,
    &CommandClasses.SecureIncludedUnSecureCC,
    &CommandClasses.SecureIncludedSecureCC
  };

  uint32_t iListLength = frame->payload[3];
  for (uint32_t i = 0; i < 3; i++)
  {
    // NOTE: These are not really supposed to be edited run time.
    // So set list lengths to 0 at first to reduce chaos if protocol
    // accesses them while we edit them.
    apCCLists[i]->iListLength = 0;

    memset((uint8_t *)(apCCLists[i]->pCommandClasses), 0, CCListSizes[i]); // Clear CCList
    memcpy((uint8_t *)(apCCLists[i]->pCommandClasses), &frame->payload[4], Minimum2(iListLength, CCListSizes[i]));

    // Set new list length after finishing CCList
    apCCLists[i]->iListLength = (uint8_t)Minimum2(iListLength, CCListSizes[i]);
  }

  SaveApplicationSettings(frame->payload[0], frame->payload[1],
                          frame->payload[2]);
  SaveApplicationCCInfo(apCCLists[0]->iListLength, (uint8_t*)apCCLists[0]->pCommandClasses,
                        0, NULL, 0, NULL);
#ifdef ZW_CONTROLLER
  ZW_UpdateCtrlNodeInformation_API_IF();
#endif

  set_state_and_notify(stateIdle);
}
#endif


#if SUPPORT_SERIAL_API_APPL_NODE_INFORMATION_CMD_CLASSES
ZW_ADD_CMD(FUNC_ID_SERIAL_API_APPL_NODE_INFORMATION_CMD_CLASSES)
{
  /* unincluded_parmLength | unincluded_nodeParm[] | */
  /* included_unsecure_parmLength | included_unsecure_nodeParm[] |*/
  /* included_secure_parmLength | included_secure_nodeParm[] */

  // Data for loopifying CC list writes
  SCommandClassList_t *const apCCLists[3] =
  {
    &CommandClasses.UnSecureIncludedCC,
    &CommandClasses.SecureIncludedUnSecureCC,
    &CommandClasses.SecureIncludedSecureCC
  };

  uint32_t iOffset = 0;
  for (uint32_t i = 0; i < 3; i++)
  {
    // NOTE: These are not really supposed to be edited run time.
    // So set list lengths to 0 at first to reduce chaos if protocol
    // accesses them while we edit them.
    apCCLists[i]->iListLength = 0;

    memset((uint8_t *)(apCCLists[i]->pCommandClasses), 0, CCListSizes[i]); // Clear CCList
    uint32_t iListLength = frame->payload[iOffset];
    memcpy((uint8_t*)apCCLists[i]->pCommandClasses, &frame->payload[iOffset + 1], Minimum2(iListLength, CCListSizes[i]));
    // Set new list length after finishing CCList
    apCCLists[i]->iListLength = (uint8_t)iListLength;

    iOffset += iListLength + 1;
  }

  const uint8_t retVal = SaveApplicationCCInfo(apCCLists[0]->iListLength, (uint8_t*)apCCLists[0]->pCommandClasses,
                                  apCCLists[1]->iListLength, (uint8_t*)apCCLists[1]->pCommandClasses,
                                  apCCLists[2]->iListLength, (uint8_t*)apCCLists[2]->pCommandClasses
                                  );
  DoRespond(retVal);
}
#endif


#if SUPPORT_SERIAL_API_APPL_SLAVE_NODE_INFORMATION
uint8_t funcID_ComplHandler_ZW_SendSlaveNodeInformation;

/*=================   ComplHandler_ZW_SendSlaveNodeInformation   ============
**    Completion handler for ZW_SendSlaveNodeInformation
**
**--------------------------------------------------------------------------*/
static void                             /* RET  Nothing                     */
ZCB_ComplHandler_ZW_SendSlaveNodeInformation(
  uint8_t txStatus,                        /* IN   Transmit completion status  */
  __attribute__((unused)) TX_STATUS_TYPE *txStatusReport)
{
  BYTE_IN_AR(compl_workbuf, 0) = funcID_ComplHandler_ZW_SendSlaveNodeInformation;
  BYTE_IN_AR(compl_workbuf, 1) = txStatus;
  Request(FUNC_ID_ZW_SEND_SLAVE_NODE_INFORMATION, compl_workbuf, 2);
}

static uint8_t SendSlaveNodeInfo(uint16_t srcNode, uint16_t destNode, uint8_t txOptions, ZW_TX_Callback_t pCallBack)
{
  SZwaveTransmitPackage FramePackage = {
      .eTransmitType = EZWAVETRANSMITTYPE_SEND_SLAVE_NODE_INFORMATION,
      .uTransmitParams.SendSlaveNodeInformation.Handle = (ZW_Void_Callback_t)pCallBack,
      .uTransmitParams.SendSlaveNodeInformation.sourceId = srcNode,
      .uTransmitParams.SendSlaveNodeInformation.destinationId = destNode,
      .uTransmitParams.SendSlaveNodeInformation.txOptions = txOptions,
  };
  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_SEND_SLAVE_NODE_INFORMATION)
{
  /* srcNode | destNode | txOptions | funcID */
  node_id_t srcNodeId, destNodeId;
  srcNodeId  = (node_id_t)frame->payload[0];
  destNodeId = (node_id_t)frame->payload[1];
  funcID_ComplHandler_ZW_SendSlaveNodeInformation = frame->payload[3];
  const uint8_t retVal = SendSlaveNodeInfo(srcNodeId, destNodeId, frame->payload[2],
                              (funcID_ComplHandler_ZW_SendSlaveNodeInformation != 0) ? &ZCB_ComplHandler_ZW_SendSlaveNodeInformation : NULL);
  DoRespond(retVal);
}

ZW_ADD_CMD(FUNC_ID_SERIAL_API_APPL_SLAVE_NODE_INFORMATION)
{
  /* srcNodeID | listening | generic | specific | parmLength | nodeParms[] */
  /* frame->payload[0] = srcNodeID */

  uint32_t iVirtualSlaveIndex = 0; // Choose which Virtual Slave node slot to use
  uint8_t offset = 0;

  // Start by removing reference to current configuration - so protocol never sees a partially modified config
  apVirtualSlaveNodeInfo[iVirtualSlaveIndex] = NULL;

  // Modify config
  aVirtualSlaveNodeInfo[iVirtualSlaveIndex].NodeId = (node_id_t)frame->payload[0];
  aVirtualSlaveNodeInfo[iVirtualSlaveIndex].bListening = frame->payload[offset + 1];
  aVirtualSlaveNodeInfo[iVirtualSlaveIndex].NodeType.generic = frame->payload[offset + 2];
  aVirtualSlaveNodeInfo[iVirtualSlaveIndex].NodeType.specific = frame->payload[offset + 3];
  aVirtualSlaveNodeInfo[iVirtualSlaveIndex].CommandClasses.iListLength = (uint8_t)Minimum2(frame->payload[offset + 4], sizeof(aVirtualSlaveCommandClasses[0]));

  memset(&aVirtualSlaveCommandClasses[iVirtualSlaveIndex][0], 0, sizeof(aVirtualSlaveCommandClasses[0]));
  memcpy(&aVirtualSlaveCommandClasses[iVirtualSlaveIndex][0], &frame->payload[offset + 5], aVirtualSlaveNodeInfo[0].CommandClasses.iListLength);

  // Re-activate config
  apVirtualSlaveNodeInfo[iVirtualSlaveIndex] = &aVirtualSlaveNodeInfo[iVirtualSlaveIndex];

  set_state_and_notify(stateIdle);
}
#endif


#if SUPPORT_ZW_SET_SLAVE_LEARN_MODE
uint8_t funcID_ComplHandler_ZW_SetSlaveLearnMode;

/*=================   ComplHandler_ZW_SetSlaveLearnMode   ====================
**    Completion handler for ZW_SetSlaveLearnMode
**
**--------------------------------------------------------------------------*/
void                                       /* RET  Nothing                     */
ZCB_ComplHandler_ZW_SetSlaveLearnMode(
  uint8_t bStatus,
  uint8_t orgID,
  uint8_t newID)                           /*  IN  Node ID                     */
{
  if (0 == funcID_ComplHandler_ZW_SetSlaveLearnMode)
  {
    return;
  }

  BYTE_IN_AR(compl_workbuf, 0) = funcID_ComplHandler_ZW_SetSlaveLearnMode;
  BYTE_IN_AR(compl_workbuf, 1) = bStatus;
  BYTE_IN_AR(compl_workbuf, 2) = orgID;
  BYTE_IN_AR(compl_workbuf, 3) = newID;
  Request(FUNC_ID_ZW_SET_SLAVE_LEARN_MODE, compl_workbuf, 4);
}

static uint8_t SetSlaveLearnMode(uint16_t nodeID, uint8_t mode)
{
  SZwaveCommandPackage cmdPackage = {
      .eCommandType = EZWAVECOMMANDTYPE_SET_SLAVE_LEARN_MODE,
      .uCommandParams.SetSlaveLearnMode.nodeID = nodeID,
      .uCommandParams.SetSlaveLearnMode.mode = mode,
  };
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&cmdPackage, 500);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
  SZwaveCommandStatusPackage cmdStatus = { 0 };
  if (GetCommandResponse(&cmdStatus, EZWAVECOMMANDSTATUS_SET_SLAVE_LEARN_MODE_RESULT))
  {
    return cmdStatus.Content.SetSlaveLearnModeStatus.result;
  }
  return false;
}

ZW_ADD_CMD(FUNC_ID_ZW_SET_SLAVE_LEARN_MODE)
{
  /* node | mode | funcID */
  uint8_t offset = 0;
  node_id_t nodeID = (node_id_t)frame->payload[0];
  funcID_ComplHandler_ZW_SetSlaveLearnMode = frame->payload[offset + 2];
  const uint8_t retVal = SetSlaveLearnMode(nodeID, frame->payload[offset + 1]);
  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_SEND_TEST_FRAME
uint8_t funcID_ComplHandler_ZW_SendTestFrame;

/*=================    ComplHandler_ZW_SendTestFrame    ====================
**    Completion handler for ZW_SendTestFrame
**
**--------------------------------------------------------------------------*/
static void
ZCB_ComplHandler_ZW_SendTestFrame(
  uint8_t txStatus,
  __attribute__((unused)) TX_STATUS_TYPE *txStatusReport)
{
  BYTE_IN_AR(compl_workbuf, 0) = funcID_ComplHandler_ZW_SendTestFrame;
  BYTE_IN_AR(compl_workbuf, 1) = txStatus;
  Request(FUNC_ID_ZW_SEND_TEST_FRAME, compl_workbuf, 2);
}

static uint8_t SendTestFrame(uint16_t nodeID, uint8_t powerLevel, ZW_TX_Callback_t pCallBack)
{
  // Create transmit frame package
  SZwaveTransmitPackage FramePackage = {
    .eTransmitType = EZWAVETRANSMITTYPE_TESTFRAME,
    .uTransmitParams.Test.DestNodeId = nodeID,
    .uTransmitParams.Test.PowerLevel = powerLevel,
    .uTransmitParams.Test.Handle = pCallBack
  };
  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}

ZW_ADD_CMD(FUNC_ID_ZW_SEND_TEST_FRAME)
{
  /* node | powerlevel | funcID */
  uint8_t offset = 0;
  node_id_t node = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  funcID_ComplHandler_ZW_SendTestFrame = frame->payload[offset + 2];
  const uint8_t retVal = SendTestFrame(node, frame->payload[offset + 1],
                          (funcID_ComplHandler_ZW_SendTestFrame != 0) ? &ZCB_ComplHandler_ZW_SendTestFrame : NULL);

  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_IS_VIRTUAL_NODE
static uint8_t IsNodeVirtual(uint16_t nodeID)
{
  if(!nodeID || (ZW_MAX_NODES < nodeID))  //Virtual nodes are not implemented for Long Range nodes
  {
    return false;
  }

  SZwaveCommandPackage cmdPackage = {
      .eCommandType = EZWAVECOMMANDTYPE_IS_VIRTUAL_NODE,
      .uCommandParams.IsVirtualNode.value = nodeID,
  };
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&cmdPackage, 500);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
  SZwaveCommandStatusPackage cmdStatus = { .eStatusType = EZWAVECOMMANDSTATUS_IS_VIRTUAL_NODE };
  if (GetCommandResponse(&cmdStatus, cmdStatus.eStatusType))
  {
    return cmdStatus.Content.IsVirtualNodeStatus.result;
  }

  assert(false); // FIXME We should have more intelligent error handling, we shouldnt assert here.
  return 0;
}

ZW_ADD_CMD(FUNC_ID_ZW_IS_VIRTUAL_NODE)
{
  /* node */
  node_id_t nodeId = (node_id_t)frame->payload[0];
  const uint8_t retVal = IsNodeVirtual(nodeId);
  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_GET_VIRTUAL_NODES
static void GetVirtualNodes(uint8_t *vNodesMask)
{
  SZwaveCommandPackage cmdPackage = {
      .eCommandType = EZWAVECOMMANDTYPE_GET_VIRTUAL_NODES};
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&cmdPackage, 500);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
  SZwaveCommandStatusPackage cmdStatus = { 0 };
  if (GetCommandResponse(&cmdStatus, EZWAVECOMMANDSTATUS_GET_VIRTUAL_NODES))
  {
    memcpy(vNodesMask, cmdStatus.Content.GetVirtualNodesStatus.vNodesMask, MAX_NODEMASK_LENGTH);
    return;
  }

  assert(false); // FIXME We should have more intelligent error handling, we shouldnt assert here.
}

ZW_ADD_CMD(FUNC_ID_ZW_GET_VIRTUAL_NODES)
{
  /* */
  GetVirtualNodes(compl_workbuf);
  DoRespond_workbuf(ZW_MAX_NODES / 8);
}
#endif

#if SUPPORT_SERIAL_API_GET_INIT_DATA
ZW_ADD_CMD(FUNC_ID_SERIAL_API_GET_INIT_DATA)
{
  uint8_t length = 0;
  func_id_serial_api_get_init_data(frame_payload_len(frame), frame->payload, compl_workbuf, &length);
  DoRespond_workbuf(length);
}
#endif
#if SUPPORT_SERIAL_API_GET_LR_NODES
#ifdef ZW_CONTROLLER
ZW_ADD_CMD(FUNC_ID_SERIAL_API_GET_LR_NODES)
{
  uint8_t length = 0;
  func_id_serial_api_get_LR_nodes(frame_payload_len(frame), frame->payload, compl_workbuf, &length);
  DoRespond_workbuf(length);
}
#endif
#endif

#if SUPPORT_SERIAL_GET_LR_CHANNEL
ZW_ADD_CMD(FUNC_ID_GET_LR_CHANNEL)
{
  GetLongRangeChannel(&compl_workbuf[0], &compl_workbuf[1]);
  DoRespond_workbuf(2);
}
#endif


#if SUPPORT_SERIAL_SET_LR_CHANNEL
ZW_ADD_CMD(FUNC_ID_SET_LR_CHANNEL)
{
  const uint8_t retVal = SetLongRangeChannel(frame->payload[0]);
  DoRespond(retVal);
}
#endif

#if SUPPORT_SERIAL_SET_LR_VIRTUAL_IDS
ZW_ADD_CMD(FUNC_ID_ZW_SET_LR_VIRTUAL_IDS)
{
  SetLongRangeVirtualNodes(frame->payload[0]);
  set_state_and_notify(stateIdle);
}
#endif


#if SUPPORT_SERIAL_ENABLE_RADIO_PTI
ZW_ADD_CMD(FUNC_ID_ENABLE_RADIO_PTI)
{
  const uint8_t retVal = SaveApplicationEnablePTI(frame->payload[0]);
  DoRespond(retVal);
}

ZW_ADD_CMD(FUNC_ID_GET_RADIO_PTI)
{
  const uint8_t retVal = GetPTIConfig();
  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_GET_CONTROLLER_CAPABILITIES
ZW_ADD_CMD(FUNC_ID_ZW_GET_CONTROLLER_CAPABILITIES)
{
  /* HOST->ZW: no params defined */
  const uint8_t retVal = GetControllerCapabilities();
  DoRespond(retVal);
}
#endif


#if SUPPORT_ZW_REQUEST_NODE_INFO
/*====================== ComplHandler_ZW_RequestNodeInfo =====================
**    Completion handler for ZW_RequestNodeInfo
**
**--------------------------------------------------------------------------*/
static void /* RET  Nothing                     */
ZCB_ComplHandler_ZW_RequestNodeInfo(
    uint8_t txStatus, /* IN   Transmit completion status  */
    __attribute__((unused)) TX_STATUS_TYPE *txStatusReport)
{
  /* */
  if (txStatus != TRANSMIT_COMPLETE_OK)
  {
    ApplicationNodeUpdate(UPDATE_STATE_NODE_INFO_REQ_FAILED, 0, NULL, 0);
  }
}

static uint8_t RequestNodeID(uint16_t nodeID)
{
   // Create transmit frame package
  SZwaveTransmitPackage FramePackage = {
    .eTransmitType = EZWAVETRANSMITTYPE_NODEINFORMATIONREQUEST,
    .uTransmitParams.NodeInfoRequest.DestNodeId = nodeID,
    .uTransmitParams.NodeInfoRequest.Handle = (ZW_Void_Callback_t)ZCB_ComplHandler_ZW_RequestNodeInfo
  };
  // Put the package on queue (and dont wait for it)
  EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwTxQueue(), (uint8_t *)&FramePackage, 0);
  return (EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus) ? true : false;
}
#if SUPPORT_ZW_REQUEST_NODE_INFO
ZW_ADD_CMD(FUNC_ID_ZW_REQUEST_NODE_INFO)
{
  /* HOST->ZW: nodeID */
  volatile uint8_t offset = 0;
  node_id_t nodeId = (node_id_t)GET_NODEID(&frame->payload[0], offset);
  const uint8_t retVal = RequestNodeID(nodeId);
  DoRespond(retVal);
}
#endif
#endif /*SUPPORT_ZW_REQUEST_NODE_INFO*/

#if SUPPORT_SERIAL_API_SET_TIMEOUTS
ZW_ADD_CMD(FUNC_ID_SERIAL_API_SET_TIMEOUTS)
{
  /* HOST->ZW: RX_ACK_timeout | RX_BYTE_timeout */
  /* ZW->HOST: RES | oldRX_ACK_timeout | oldRX_BYTE_timeout */
  BYTE_IN_AR(compl_workbuf, 0) = (uint8_t)(comm_interface_get_ack_timeout_ms() / 10); /* Respond with the old timeout settings */
  BYTE_IN_AR(compl_workbuf, 1) = (uint8_t)(comm_interface_get_byte_timeout_ms() / 10);
  comm_interface_set_ack_timeout_ms(frame->payload[0] * 10);  /* Max time to wait for ACK after frame transmission in 10ms ticks */
  comm_interface_set_byte_timeout_ms(frame->payload[1] * 10); /* Max time to wait for next byte when collecting a new frame in 10ms ticks */
  /* Respond with the old timeout settings */
  DoRespond_workbuf(2);
}
#endif


#if SUPPORT_SERIAL_API_SOFT_RESET
ZW_ADD_CMD(FUNC_ID_SERIAL_API_SOFT_RESET)
{
  //Enqueue soft reset command to protocol
  SZwaveCommandPackage Command = {
    .eCommandType = EZWAVECOMMANDTYPE_SOFT_RESET
  };
  QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&Command, 0);
}
#endif


#if SUPPORT_SERIAL_API_SETUP
ZW_ADD_CMD(FUNC_ID_SERIAL_API_SETUP)
{
  /* HOST->ZW: Cmd | [CmdData[]] */
  /* ZW->HOST: Cmd | CmdRes[] */
  uint8_t length;
  func_id_serial_api_setup(frame_payload_len(frame), frame->payload, compl_workbuf, &length);
  DoRespond_workbuf(length);
}
#endif

#if SUPPORT_SERIAL_API_READY
ZW_ADD_CMD(FUNC_ID_SERIAL_API_READY)
{
  /* HOST->ZW: [serialLinkState] */
  /* serialLinkState is OPTIONAL, if not present then the READY command, means "READY" */
  if ((FRAME_LENGTH_MIN < frame->len) && (SERIAL_LINK_DETACHED == frame->payload[0]))
  {
    /* HOST want Serial link to be shutdown - SerialAPI will not transmit anything */
    /* via serial link as long as HOST do not reestablish the Serial Link by */
    /* transmitting a valid SerialAPI frame. */
    serialLinkState = SERIAL_LINK_DETACHED;
    /* Purge Callback queue */
    PurgeCallbackQueue();
    PurgeCommandQueue();
  }
  else
  {
    /* Missing serialLinkState parameter or Every other serialLinkState value means */
    /* HOST Ready for Serial Communication */
    serialLinkState = SERIAL_LINK_CONNECTED;
  }
  set_state_and_notify(stateIdle);
}
#endif


#if SUPPORT_ZW_TYPE_LIBRARY
ZW_ADD_CMD(FUNC_ID_ZW_TYPE_LIBRARY)
{
  const SProtocolInfo* protocol_info = ZAF_getProtocolInfo();
  DoRespond(protocol_info->eLibraryType);
}
#endif


#if SUPPORT_ZW_WATCHDOG_START | SUPPORT_ZW_WATCHDOG_STOP
uint8_t bWatchdogStarted;
#endif


#if SUPPORT_ZW_WATCHDOG_START
ZW_ADD_CMD(FUNC_ID_ZW_WATCHDOG_START)
{
  bWatchdogStarted = true;
  zpal_enable_watchdog(true);
  set_state_and_notify(stateIdle);
}
#endif


#if SUPPORT_ZW_WATCHDOG_STOP
ZW_ADD_CMD(FUNC_ID_ZW_WATCHDOG_STOP)
{
  zpal_enable_watchdog(false);
  bWatchdogStarted = false;
  set_state_and_notify(stateIdle);
}
#endif


#if SUPPORT_ZW_SET_ROUTING_MAX
static void zw_set_routing_max_handler(const comm_interface_frame_ptr frame)
{
  SZwaveCommandPackage Command = {
    .eCommandType = EZWAVECOMMANDTYPE_SET_ROUTING_MAX,
    .uCommandParams.SetRoutingMax.value = frame->payload[0]
  };
  QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&Command, 0);
  DoRespond(1);
}

ZW_ADD_CMD(FUNC_ID_ZW_SET_ROUTING_MAX)
{
  zw_set_routing_max_handler(frame);
}

ZW_ADD_CMD(FUNC_ID_ZW_SET_ROUTING_MAX_6_00)
{
  zw_set_routing_max_handler(frame);
}
#endif

#if SUPPORT_SERIAL_API_EXT
ZW_ADD_CMD(FUNC_ID_SERIAL_API_EXT)
{
  /* HOST->ZW: mode | data */
  /* not used in 700 series - Obsolete */
  if (frame->len > FRAME_LENGTH_MIN)
  {
    switch (frame->payload[0])
    {
      /* since the 700 / 800 series targets don't support the command,
          then all values of the mode parameters should return zero*/
      case 0:
      case 1:
      {
        BYTE_IN_AR(compl_workbuf, 0) = 0;
        DoRespond_workbuf(1);
      }
      break;
      default:
      {
        DoRespond(0);
      }
      break;
    }
  }
}
#endif


#if SUPPORT_ZW_GET_RANDOM
static uint8_t GetRandom(uint8_t noOfRndBytes, uint8_t* rndBytes)
{
  SZwaveCommandPackage GetRandom = {
      .eCommandType = EZWAVECOMMANDTYPE_GENERATE_RANDOM,
      .uCommandParams.GenerateRandom.iLength = noOfRndBytes};

  // Put the Command on queue (and dont wait for it, queue must be empty)
  if (EQUEUENOTIFYING_STATUS_SUCCESS == QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&GetRandom, 0))
  {
    // Wait for protocol to handle command
    SZwaveCommandStatusPackage Random = { .eStatusType = EZWAVECOMMANDSTATUS_GENERATE_RANDOM };
    if (GetCommandResponse(&Random, Random.eStatusType))
    {
      memcpy(rndBytes, Random.Content.GenerateRandomStatus.aRandomNumber, Random.Content.GenerateRandomStatus.iLength);
      return  Random.Content.GenerateRandomStatus.iLength;
    }
  }
  return false;
}

ZW_ADD_CMD(FUNC_ID_ZW_GET_RANDOM)
{
  /* HOST->ZW: noRandomBytes - Optional if not present or equal ZERO then  */
  /*                           2 random bytes are returned.                */
  /*                           Range 1..32 random bytes are supported      */
  /* ZW->HOST: RES | randomGenerationSuccess | noRandomBytesGenerated | noRandomGenerated[] */
  uint8_t noRndBytes = frame->payload[0];
  if ((frame->len > FRAME_LENGTH_MIN) && (noRndBytes != 0))
  {
    if (noRndBytes > 32)
    {
      noRndBytes = 32;
    }
  }
  else
  {
    noRndBytes = 2;
  }
  // Prepare failed return
  BYTE_IN_AR(compl_workbuf, 0) = false;
  BYTE_IN_AR(compl_workbuf, 1) = 0;
  uint8_t rndBytes = GetRandom(noRndBytes, &compl_workbuf[2]);
  if (rndBytes)
  {
    BYTE_IN_AR(compl_workbuf, 0) = true;
    BYTE_IN_AR(compl_workbuf, 1) = rndBytes;
  }
  rndBytes += 2;
  DoRespond_workbuf(rndBytes);
}
#endif


#if SUPPORT_ZW_AES_ECB
static void AesEcb(uint8_t *key, uint8_t *InputData, uint8_t *outData)
{
  SZwaveCommandPackage cmdPackage = {.eCommandType = EZWAVECOMMANDTYPE_AES_ECB};
  memcpy(cmdPackage.uCommandParams.AesEcb.key, key, sizeof(cmdPackage.uCommandParams.AesEcb.key));
  memcpy(cmdPackage.uCommandParams.AesEcb.inputData, InputData, sizeof(cmdPackage.uCommandParams.AesEcb.inputData));
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&cmdPackage, 500);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
  SZwaveCommandStatusPackage cmdStatus = { 0 };
  if (GetCommandResponse(&cmdStatus, EZWAVECOMMANDSTATUS_AES_ECB))
  {
    memcpy(outData, cmdStatus.Content.AesEcbStatus.outputData, 16);
    return;

  }
  assert(false); // FIXME We should have more intelligent error handling, we shouldnt assert here.
}

ZW_ADD_CMD(FUNC_ID_ZW_AES_ECB)
{
  /* HOST->ZW: key[] (16 bytes) | inputDat[] (16 bytes) */
  /* ZW->HOST: RES | outdata[] (16 bytes) */
  AesEcb(&frame->payload[0], &frame->payload[16], compl_workbuf);
  DoRespond_workbuf(16);
}
#endif


#if SUPPORT_ZW_AUTO_PROGRAMMING
ZW_ADD_CMD(FUNC_ID_AUTO_PROGRAMMING)
{
  //Reboot into bootloader.  Calls zpal_bootloader_reboot_and_install();
  SZwaveCommandPackage Command = {
    .eCommandType = EZWAVECOMMANDTYPE_BOOTLOADER_REBOOT
  };
  QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&Command, 0);
}
#endif


#ifdef SUPPORT_ZW_SET_LISTEN_BEFORE_TALK_THRESHOLD
ZW_ADD_CMD(FUNC_ID_ZW_SET_LISTEN_BEFORE_TALK_THRESHOLD)
{
  /* HOST->ZW: bChannel | bThreshold */
  /* ZW->HOST: RES | true */
  uint8_t length;
  func_id_set_listen_before_talk(frame_payload_len(frame), frame->payload, compl_workbuf, &length);
  DoRespond_workbuf(length);
}
#endif


#ifdef SUPPORT_ZW_NETWORK_MANAGEMENT_SET_MAX_INCLUSION_REQUEST_INTERVALS
static bool SetMaxInclReqIntervals( uint32_t maxInclReqIntervals)
{
  SZwaveCommandPackage setMaxInclusionRequestIntervals = {
    .eCommandType = EZWAVECOMMANDTYPE_ZW_SET_MAX_INCL_REQ_INTERVALS,
    .uCommandParams.SetMaxInclReqInterval.inclusionRequestInterval = maxInclReqIntervals};

  // Put the Command on queue (and dont wait for it, queue must be empty)
  if (EQUEUENOTIFYING_STATUS_SUCCESS == QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&setMaxInclusionRequestIntervals, 0))
  {
    // Wait for protocol to handle command
    SZwaveCommandStatusPackage result = { .eStatusType = EZWAVECOMMANDSTATUS_ZW_SET_MAX_INCL_REQ_INTERVALS};
    if ((GetCommandResponse(&result, result.eStatusType))
      && (result.Content.NetworkManagementStatus.statusInfo[0]))
    {
      return true;
    }
  }
  return false;
}

ZW_ADD_CMD(FUNC_ID_ZW_NETWORK_MANAGEMENT_SET_MAX_INCLUSION_REQUEST_INTERVALS)
{
  /* HOST->ZW: bChannel | bThreshold */
  /* ZW->HOST: RES | true */
  const uint8_t retVal = SetMaxInclReqIntervals(frame->payload[0]);
  DoRespond(retVal);
}
#endif

// Added to make sure that capabilities is correct.
ZW_ADD_CMD(FUNC_ID_SERIAL_API_STARTED)
{
}

// Added to make sure that capabilities is correct.
ZW_ADD_CMD(FUNC_ID_ZW_APPLICATION_UPDATE)
{
}

// Added to make sure that capabilities is correct.
ZW_ADD_CMD(FUNC_ID_APPLICATION_COMMAND_HANDLER_BRIDGE)
{
}

// Added to make sure that capabilities is correct.
ZW_ADD_CMD(FUNC_ID_ZW_GET_PROTOCOL_STATUS)
{
}
