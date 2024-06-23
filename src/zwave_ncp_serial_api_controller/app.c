/**
 * @file
 * Serial API implementation for Enhanced Z-Wave module
 *
 * @copyright 2019 Silicon Laboratories Inc.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "SyncEvent.h"
#ifdef ZW_CONTROLLER
#include "ZW_controller_api.h"
#endif /* ZW_CONTROLLER */
#include "AppTimer.h"
#include "ZW_system_startup_api.h"
/* Include app header file - containing version and */
/* SerialAPI functionality support definitions */
#include "DebugPrintConfig.h"
// SerialAPI uses SWO for debug output.
// For example SWO Terminal in  Studio commander can be used to get the output.
//#define DEBUGPRINT
#include "DebugPrint.h"
#include "app_node_info.h"
#include "serialapi_file.h"
#include "cmd_handlers.h"
#include "cmds_management.h"
#include "ZAF_Common_interface.h"
#include "utils.h"
#include "app_hw.h"
#include "SerialAPI_hw.h"
#include "zaf_event_distributor_ncp.h"
#include "zpal_misc.h"
#include "zaf_protocol_config.h"
#ifdef DEBUGPRINT
#include "ZAF_PrintAppInfo.h"
#endif
#include "ZAF_AppName.h"

#include <assert.h>

/* Basic level definitions */
#define BASIC_ON 0xFF
#define BASIC_OFF 0x00

#define TX_POWER_LR_20_DBM    200
#define TX_POWER_LR_14_DBM    140

#define REQUESTED_SECURITY_KEYS   0

/* Accept all incoming command classes, regardless of NIF contents. */
#define ACCEPT_ALL_CMD_CLASSES

/**
 *
 */
typedef struct _S_TRANSPORT_REQUESTED_SECURITY_SETTINGS_
{
  uint8_t requestedSecurityKeysBits;
} S_TRANSPORT_REQUESTED_SECURITY_SETTINGS;

static TaskHandle_t g_AppTaskHandle;

extern SSyncEvent SetDefaultCB;
extern SSyncEventArg1 LearnModeStatusCb;

/* State vars for ApplicationPoll */
static uint8_t state = 0xff;
static uint8_t retry = 0;

static uint8_t lastRetVal = 0;      /* Used to store retVal for retransmissions */
uint8_t compl_workbuf[BUF_SIZE_TX]; /* Used for frames send to remote side. */

/* Queue for frames transmitted to PC - callback, ApplicationCommandHandler, ApplicationControllerUpdate... */
#if !defined(MAX_CALLBACK_QUEUE)
#define MAX_CALLBACK_QUEUE  8
#endif /* !defined(MAX_CALLBACK_QUEUE) */

#if !defined(MAX_UNSOLICITED_QUEUE)
#define MAX_UNSOLICITED_QUEUE 8
#endif /* !defined(MAX_UNSOLICITED_QUEUE) */

typedef struct _callback_element_
{
  uint8_t wCmd;
  uint8_t wLen;
  uint8_t wBuf[BUF_SIZE_TX];
} CALLBACK_ELEMENT;

typedef struct _request_queue_
{
  uint8_t requestOut;
  uint8_t requestIn;
  uint8_t requestCnt;
  CALLBACK_ELEMENT requestQueue[MAX_CALLBACK_QUEUE];
} REQUEST_QUEUE;

REQUEST_QUEUE callbackQueue = {0};

typedef struct _request_unsolicited_queue_
{
  uint8_t requestOut;
  uint8_t requestIn;
  uint8_t requestCnt;
  CALLBACK_ELEMENT requestQueue[MAX_UNSOLICITED_QUEUE];
} REQUEST_UNSOLICITED_QUEUE;

REQUEST_UNSOLICITED_QUEUE commandQueue = {0};

eSerialAPISetupNodeIdBaseType nodeIdBaseType = SERIAL_API_SETUP_NODEID_BASE_TYPE_DEFAULT;

#if SUPPORT_ZW_WATCHDOG_START | SUPPORT_ZW_WATCHDOG_STOP
extern uint8_t bWatchdogStarted;
#endif

/* Last system wakeup reason - is set in ApplicationInit */
zpal_reset_reason_t g_eApplResetReason;

zpal_pm_handle_t radio_power_lock;
zpal_pm_handle_t io_power_lock;
SSwTimer mWakeupTimer;
bool bTxStatusReportEnabled;

static void ApplicationInitSW(void);
static void ApplicationTask(SApplicationHandles *pAppHandles);

#ifdef ZW_CONTROLLER_BRIDGE
static void ApplicationCommandHandler_Bridge(SReceiveMulti *pReciveMulti);
#else
void ApplicationCommandHandler(void *pSubscriberContext, SZwaveReceivePackage* pRxPackage);
#endif

void ApplicationNodeUpdate(uint8_t bStatus, uint16_t nodeID, uint8_t *pCmd, uint8_t bLen);

#if SUPPORT_ZW_REMOVE_FAILED_NODE_ID
extern void ZCB_ComplHandler_ZW_RemoveFailedNodeID(uint8_t bStatus);
#endif

#if SUPPORT_ZW_REPLACE_FAILED_NODE
extern void ZCB_ComplHandler_ZW_ReplaceFailedNode(uint8_t bStatus);
#endif

#if SUPPORT_ZW_SET_SLAVE_LEARN_MODE
extern void ZCB_ComplHandler_ZW_SetSlaveLearnMode(uint8_t bStatus, uint8_t orgID, uint8_t newID);
#endif

#if SUPPORT_ZW_SET_RF_RECEIVE_MODE
extern uint8_t SetRFReceiveMode(uint8_t mode);
#endif

void set_state_and_notify(uint8_t st)
{
  if (state != st)
  {
    xTaskNotify(g_AppTaskHandle,
                1<<EAPPLICATIONEVENT_STATECHANGE,
                eSetBits );
    state = st;
  }
}

void set_state(uint8_t st)
{
  state = st;
}

/*===============================   Request   ================================
**    Queues request (callback) to be transmitted to remote side
**
**--------------------------------------------------------------------------*/
bool /*RET  queue status (false queue full)*/
Request(
    uint8_t cmd,       /*IN   Command                  */
    uint8_t *pData, /*IN   pointer to data          */
    uint8_t len        /*IN   Length of data           */
)
{
#if SUPPORT_SERIAL_API_READY
  /* Only queue Request frame for HOST if SERIAL LINK has been established or we need to send the WakeUp Frame */
  if (((SERIAL_LINK_DETACHED != serialLinkState) || (wakeUpOnRF)) && (callbackQueue.requestCnt < MAX_CALLBACK_QUEUE))
#else
  if (callbackQueue.requestCnt < MAX_CALLBACK_QUEUE)
#endif
  {
    callbackQueue.requestCnt++;
    callbackQueue.requestQueue[callbackQueue.requestIn].wCmd = cmd;
    if (len > (uint8_t)BUF_SIZE_TX)
    {
      assert((uint8_t)BUF_SIZE_TX >= len);
      len = (uint8_t)BUF_SIZE_TX;
    }
    callbackQueue.requestQueue[callbackQueue.requestIn].wLen = len;
    memcpy(&callbackQueue.requestQueue[callbackQueue.requestIn].wBuf[0], pData, len);
    if (++callbackQueue.requestIn >= MAX_CALLBACK_QUEUE)
    {
      callbackQueue.requestIn = 0;
    }
    xTaskNotify(g_AppTaskHandle,
                1<<EAPPLICATIONEVENT_STATECHANGE,
                eSetBits);

    return true;
  }
  return false;
}

/*=========================   RequestUnsolicited   ===========================
**    Queues request (command) to be transmitted to remote side
**
**--------------------------------------------------------------------------*/
bool /*RET  queue status (false queue full)*/
RequestUnsolicited(
    uint8_t cmd,       /*IN   Command                  */
    uint8_t *pData, /*IN   pointer to data          */
    uint8_t len        /*IN   Length of data           */
)
{
  taskENTER_CRITICAL();
#if SUPPORT_SERIAL_API_READY
  /* Only queue Request frame for HOST if SERIAL LINK has been established or we need to send the WakeUp Frame */
  if (((SERIAL_LINK_DETACHED != serialLinkState) || (wakeUpOnRF)) && (commandQueue.requestCnt < MAX_UNSOLICITED_QUEUE))
#else
  if (commandQueue.requestCnt < MAX_UNSOLICITED_QUEUE)
#endif
  {
    commandQueue.requestCnt++;
    commandQueue.requestQueue[commandQueue.requestIn].wCmd = cmd;
    if (len > (uint8_t)BUF_SIZE_TX)
    {
      assert((uint8_t)BUF_SIZE_TX >= len);
      len = (uint8_t)BUF_SIZE_TX;
    }
    commandQueue.requestQueue[commandQueue.requestIn].wLen = len;
    memcpy(&commandQueue.requestQueue[commandQueue.requestIn].wBuf[0], pData, len);
    if (++commandQueue.requestIn >= MAX_UNSOLICITED_QUEUE)
    {
      commandQueue.requestIn = 0;
    }
    taskEXIT_CRITICAL();
    xTaskNotify(g_AppTaskHandle,
                1<<EAPPLICATIONEVENT_STATECHANGE,
                eSetBits);
    return true;
  }
  taskEXIT_CRITICAL();
  return false;
}

void PurgeCallbackQueue(void)
{
  callbackQueue.requestOut = callbackQueue.requestIn = callbackQueue.requestCnt = 0;
}

void PurgeCommandQueue(void)
{
  taskENTER_CRITICAL();
  commandQueue.requestOut = commandQueue.requestIn = commandQueue.requestCnt = 0;
  taskEXIT_CRITICAL();
}

/*===============================   Respond   ===============================
**    Send immediate respons to remote side
**
**    Side effects: Sets state variable to stateTxSerial (wait for ack)
**
**--------------------------------------------------------------------------*/
void /*RET  Nothing                 */
Respond(
    uint8_t cmd,             /*IN   Command                  */
    uint8_t const *pData, /*IN   pointer to data          */
    uint8_t len              /*IN   Length of data           */
)
{
  /* If there are no data; pData == NULL and len == 0 we must set the data pointer */
  /* to some dummy data. comm_interface_transmit_frame interprets NULL pointer as retransmit indication */
  if (len == 0)
  {
    pData = (uint8_t *)0x7ff; /* Just something is not used anyway */
  }
  comm_interface_transmit_frame(cmd, RESPONSE, pData, len, NULL);

  set_state_and_notify(stateTxSerial); /* We want ACK/NAK...*/
}

void
DoRespond(uint8_t retVal)
{
  /* We need to store retVal for retransmission. */
  lastRetVal = retVal;
  Respond(serial_frame->cmd, &lastRetVal, 1);
}

void
DoRespond_workbuf(
  uint8_t cnt
)
{
  Respond(serial_frame->cmd, compl_workbuf, cnt);
}

void zaf_event_distributor_app_zw_rx(SZwaveReceivePackage *RxPackage)
{
  switch (RxPackage->eReceiveType) {
    case EZWAVERECEIVETYPE_SINGLE:
#ifndef ZW_CONTROLLER_BRIDGE
      ApplicationCommandHandler(NULL, RxPackage);
#endif
      break;

#ifdef ZW_CONTROLLER_BRIDGE
    case EZWAVERECEIVETYPE_MULTI:
      ApplicationCommandHandler_Bridge(&RxPackage->uReceiveParams.RxMulti);
      break;
#endif // #ifdef ZW_CONTROLLER_BRIDGE

    case EZWAVERECEIVETYPE_NODE_UPDATE:
      ApplicationNodeUpdate(
        RxPackage->uReceiveParams.RxNodeUpdate.Status,
        RxPackage->uReceiveParams.RxNodeUpdate.NodeId,
        RxPackage->uReceiveParams.RxNodeUpdate.aPayload,
        RxPackage->uReceiveParams.RxNodeUpdate.iLength);
      break;
    default:
      break;
  }
}


/**
* @brief Triggered when protocol puts a message on the ZwCommandStatusQueue.
*/
void zaf_event_distributor_app_zw_command_status(SZwaveCommandStatusPackage *Status)
{
  DPRINTF("Incoming Status msg %x\r\n", Status->eStatusType);

  switch (Status->eStatusType) {
    case EZWAVECOMMANDSTATUS_LEARN_MODE_STATUS:
      SyncEventArg1Invoke(&LearnModeStatusCb, Status->Content.LearnModeStatus.Status);
      break;

    case EZWAVECOMMANDSTATUS_SET_DEFAULT:
      // Received when protocol is started (not implemented yet), and when SetDefault command is completed
      SyncEventInvoke(&SetDefaultCB);
      break;

#ifdef ZW_CONTROLLER
    case EZWAVECOMMANDSTATUS_REPLACE_FAILED_NODE_ID:
      ZCB_ComplHandler_ZW_ReplaceFailedNode(Status->Content.FailedNodeIDStatus.result);
      break;
    case EZWAVECOMMANDSTATUS_REMOVE_FAILED_NODE_ID:
      ZCB_ComplHandler_ZW_RemoveFailedNodeID(Status->Content.FailedNodeIDStatus.result);
      break;
    case EZWAVECOMMANDSTATUS_NETWORK_MANAGEMENT:
    {
      LEARN_INFO_T mLearnInfo;
      mLearnInfo.bStatus = Status->Content.NetworkManagementStatus.statusInfo[0];
      mLearnInfo.bSource = (uint16_t)(((uint16_t)Status->Content.NetworkManagementStatus.statusInfo[1] << 8)  // nodeID MSB
                                      | (uint16_t)Status->Content.NetworkManagementStatus.statusInfo[2]);     // nodeID LSB
      mLearnInfo.bLen = Status->Content.NetworkManagementStatus.statusInfo[3];
      mLearnInfo.pCmd = &Status->Content.NetworkManagementStatus.statusInfo[4];
      ZCB_ComplHandler_ZW_NodeManagement(&mLearnInfo);
      break;
    }
#if SUPPORT_ZW_SET_SLAVE_LEARN_MODE
    case EZWAVECOMMANDSTATUS_SET_SLAVE_LEARN_MODE:
    {
      uint8_t bStatus;
      uint16_t orgID;
      uint16_t newID;
      bStatus = Status->Content.NetworkManagementStatus.statusInfo[0];
      orgID = (uint16_t)((uint16_t)(Status->Content.NetworkManagementStatus.statusInfo[1] << 8)   // org nodeID MSB
                         | Status->Content.NetworkManagementStatus.statusInfo[2]);                // org nodeID LSB
      newID = (uint16_t)((uint16_t)(Status->Content.NetworkManagementStatus.statusInfo[3] << 8)   // new nodeID MSB
                         | Status->Content.NetworkManagementStatus.statusInfo[4]);                // new nodeID LSB
      ZCB_ComplHandler_ZW_SetSlaveLearnMode(bStatus, (uint8_t)orgID, (uint8_t)newID);            // orgID and newID are always (8-bit) IDs
      break;
    }
#endif
#endif
    default:
      break;
  }
}

static void
appFileSystemInit(void)
{
  SAppNodeInfo_t *AppNodeInfo;
  SRadioConfig_t *RadioConfig;

  AppNodeInfo = zaf_get_app_node_info();
  RadioConfig = zaf_get_radio_config();

  /*
   * Handle file system init inside Application Task
   * This reduces the default stack needed during initialization
   */
  if (SerialApiFileInit())
  {
    ReadApplicationSettings(&AppNodeInfo->DeviceOptionsMask, &AppNodeInfo->NodeType.generic, &AppNodeInfo->NodeType.specific);
    ReadApplicationCCInfo(&CommandClasses.UnSecureIncludedCC.iListLength,
                          (uint8_t*)CommandClasses.UnSecureIncludedCC.pCommandClasses,
                          &CommandClasses.SecureIncludedUnSecureCC.iListLength,
                          (uint8_t*)CommandClasses.SecureIncludedUnSecureCC.pCommandClasses,
                          &CommandClasses.SecureIncludedSecureCC.iListLength,
                          (uint8_t*)CommandClasses.SecureIncludedSecureCC.pCommandClasses);
    ReadApplicationRfRegion(&RadioConfig->eRegion);
    ReadApplicationTxPowerlevel(&RadioConfig->iTxPowerLevelMax, &RadioConfig->iTxPowerLevelAdjust);
    ReadApplicationMaxLRTxPwr(&RadioConfig->iTxPowerLevelMaxLR);
    ReadApplicationEnablePTI(&RadioConfig->radio_debug_enable);
    ReadApplicationNodeIdBaseType(&nodeIdBaseType);
  }
  else
  {
    /*
     * We end up here on the first boot after initializing the flash file system
     */

    zpal_radio_region_t mfgRegionConfig = REGION_UNDEFINED;
    // In case of valid MfgToken, override the app default settings.
    ZW_GetMfgTokenDataCountryFreq(&mfgRegionConfig);
    if (true == isRfRegionValid(mfgRegionConfig))
    {
      RadioConfig->eRegion = mfgRegionConfig;
    }

    // Save the setting to flash
    SaveApplicationRfRegion(RadioConfig->eRegion);
    // Save the default Tx powerlevel
    SaveApplicationTxPowerlevel(RadioConfig->iTxPowerLevelMax, RadioConfig->iTxPowerLevelAdjust);
    // write defualt values
    SaveApplicationSettings(AppNodeInfo->DeviceOptionsMask, AppNodeInfo->NodeType.generic, AppNodeInfo->NodeType.specific);
    // change the 20dBm tx power setting according to the application configuration
    SaveApplicationMaxLRTxPwr(RadioConfig->iTxPowerLevelMaxLR);

    SaveApplicationEnablePTI(RadioConfig->radio_debug_enable);
    SaveApplicationNodeIdBaseType(SERIAL_API_SETUP_NODEID_BASE_TYPE_DEFAULT);
  }

  ZAF_AppName_Write();
}
/*
 * The below function must be implemented as hardware specific function in a separate source
 * file if required.
 */
ZW_WEAK void SerialAPI_hw_psu_init(void)
{
  // Do nothing
}

/*===============================   ApplicationPoll   =======================
**    Application poll function, handling the receiving and transmitting
**    communication with the PC.
**
**--------------------------------------------------------------------------*/
static void           /*RET  Nothing                  */
ApplicationTask(SApplicationHandles* pAppHandles)
{

  SerialAPI_hw_psu_init(); // Must be invoked after the file system is initialized.

  // Init
  g_AppTaskHandle = xTaskGetCurrentTaskHandle();

  SetTaskHandle(g_AppTaskHandle);
  ZAF_setAppHandle(pAppHandles);
  ZW_system_startup_SetCCSet(&CommandClasses);

  AppTimerInit(EAPPLICATIONEVENT_TIMER, (void *) g_AppTaskHandle);
  radio_power_lock = zpal_pm_register(ZPAL_PM_TYPE_USE_RADIO);
  zpal_pm_stay_awake(radio_power_lock, 0);
  io_power_lock = zpal_pm_register(ZPAL_PM_TYPE_DEEP_SLEEP);
  zpal_pm_stay_awake(io_power_lock, 0);

  zaf_event_distributor_init();

  set_state_and_notify(stateStartup);
  // Wait for and process events
  DPRINT("SerialApi Event processor Started\r\n");
  for (;;)
  {
    if (!zaf_event_distributor_distribute())
    {
      return;
    }
  }
}

static void SerialAPICommandHandler(void)
{
  const bool handler_invoked = invoke_cmd_handler(serial_frame);
  if (!handler_invoked)
  {
    /* TODO - send a "Not Supported" respond frame */
    /* UNKNOWN - just drop it */
    set_state_and_notify(stateIdle);
  }
}


static void SerialAPIStateHandler(void)
{
  comm_interface_parse_result_t conVal;

  /* ApplicationPoll is controlled by a statemachine with the four states:
      stateIdle, stateFrameParse, stateTxSerial, stateCbTxSerial.

      stateIdle: If there is anything to transmit do so. -> stateCbTxSerial
                 If not, check if anything is received. -> stateFrameParse
                 If neither, stay in the state
                 Note: frames received while we are transmitting are lost
                 and must be retransmitted by PC

      stateFrameParse: Parse received frame.
                 If the request has no response -> stateIdle
                 If there is an immediate response send it. -> stateTxSerial

      stateTxSerial:  Waits for ack on responses send in stateFrameParse.
                 Retransmit frame as needed.
                 -> stateIdle

      stateCbTxSerial:  Waits for ack on requests send in stateIdle
                  (callback, ApplicationCommandHandler etc).
                 Retransmit frame as needed and remove from callbackqueue when done.
                 -> stateIdle

	  stateAppSuspend: Added for the uzb suspend function. The resume is through the suspend signal goes high in UZB stick
	                   The wakeup from deep sleep suspend causes system reboot

  */

  {
    switch (state)
    {
      case stateStartup:
        {
          ApplicationInitSW();
          SetRFReceiveMode(1);
          set_state_and_notify(stateIdle);
        }
        break;

      case stateIdle:
        {
#if SUPPORT_SERIAL_API_READY
          /* Only empty callback queue for HOST if SERIAL LINK has been established  */
          if (callbackQueue.requestCnt && (SERIAL_LINK_DETACHED != serialLinkState))
#else
          /* Check if there is anything to transmit. If so do it */
          if (callbackQueue.requestCnt)
#endif
          {
            comm_interface_transmit_frame(
              callbackQueue.requestQueue[callbackQueue.requestOut].wCmd,
              REQUEST,
              (uint8_t *)callbackQueue.requestQueue[callbackQueue.requestOut].wBuf,
              callbackQueue.requestQueue[callbackQueue.requestOut].wLen,
              NULL
            );
            set_state_and_notify(stateCallbackTxSerial);
            /* callbackCnt decremented when frame is acknowledged from PC - or timed out after retries */
          }
          else
          {
#if SUPPORT_SERIAL_API_READY
            /* Only empty command queue for HOST if SERIAL LINK has been established  */
            if (commandQueue.requestCnt && (SERIAL_LINK_DETACHED != serialLinkState))
#else
            /* Check if there is anything to transmit. If so do it */
            if (commandQueue.requestCnt)
#endif
            {
              comm_interface_transmit_frame(
                commandQueue.requestQueue[commandQueue.requestOut].wCmd,
                REQUEST,
                (uint8_t *)commandQueue.requestQueue[commandQueue.requestOut].wBuf,
                commandQueue.requestQueue[commandQueue.requestOut].wLen,
                NULL
              );
              set_state_and_notify(stateCommandTxSerial);
              /* commandCnt decremented when frame is acknowledged from PC - or timed out after retries */
            }
            else
            {
              /* Nothing to transmit. Check if we received anything */
              if (comm_interface_parse_data(true) == PARSE_FRAME_RECEIVED)
              {
#if SUPPORT_SERIAL_API_READY
                /* We have received a frame from HOST so we must assume we are connected */
                serialLinkState = SERIAL_LINK_CONNECTED;
#endif
                /* We got a frame... */
                set_state_and_notify(stateFrameParse);
              }
            }
          }
        }
        break;

    case stateFrameParse:
      {
        SerialAPICommandHandler();
      }
      break;

    case stateTxSerial:
    {
      /* Wait for ACK on send respond. Retransmit as needed */
      if ((conVal = comm_interface_parse_data(false)) == PARSE_FRAME_SENT)
      {
        /* One more RES transmitted succesfully */
        retry = 0;
        set_state_and_notify(stateIdle);
      }
      else if (conVal == PARSE_TX_TIMEOUT)
      {
        /* Either a NAK has been received or we timed out waiting for ACK */
        if (retry++ < MAX_SERIAL_RETRY)
        {
          comm_interface_transmit_frame(0, REQUEST, NULL, 0, NULL); /* Retry... */
        }
        else
        {
          /* Drop RES as HOST could not be reached */
          retry = 0;
          set_state_and_notify(stateIdle);
        }
      }
      /* All other states are ignored, as for now the only thing we are looking for is ACK/NAK! */
    }
    break;

    case stateCallbackTxSerial:
    {
      /* Wait for ack on unsolicited event (callback etc.) */
      /* Retransmit as needed. Remove frame from callbackQueue when done */
      if ((conVal = comm_interface_parse_data(false)) == PARSE_FRAME_SENT)
      {
        /* One more REQ transmitted succesfully */
        PopCallBackQueue();
      }
      else if (conVal == PARSE_TX_TIMEOUT)
      {
        /* Either a NAK has been received or we timed out waiting for ACK */
        if (retry++ < MAX_SERIAL_RETRY)
        {
          comm_interface_transmit_frame(0, REQUEST, NULL, 0, NULL); /* Retry... */
        }
        else
        {
          /* Drop REQ as HOST could not be reached */
          PopCallBackQueue();
        }
      }
      /* All other states are ignored, as for now the only thing we are looking for is ACK/NAK! */
    }
    break;

    case stateCommandTxSerial:
    {
      /* Wait for ack on unsolicited ApplicationCommandHandler event */
      /* Retransmit as needed. Remove frame from comamndQueue when done */
      if ((conVal = comm_interface_parse_data(false)) == PARSE_FRAME_SENT)
      {
        /* One more REQ transmitted succesfully */
        PopCommandQueue();
      }
      else if (conVal == PARSE_TX_TIMEOUT)
      {
        /* Either a NAK has been received or we timed out waiting for ACK */
        if (retry++ < MAX_SERIAL_RETRY)
        {
          comm_interface_transmit_frame(0, REQUEST, NULL, 0, NULL); /* Retry... */
        }
        else
        {
          /* Drop REQ as HOST could not be reached */
          PopCommandQueue();
        }
      }
      /* All other states are ignored, as for now the only thing we are looking for is ACK/NAK! */
    }
    break;
 #ifdef USB_SUSPEND_SUPPORT
	case stateAppSuspend:
		SerialAPI_hw_usb_suspend_handler();
	break;
#endif
    default:
      set_state_and_notify(stateIdle);
      break;
    }

  } // For loop - task loop
}


void
zaf_event_distributor_app_state_change(void)
{
  SerialAPIStateHandler();
}

void
zaf_event_distributor_app_serial_data_rx(void)
{
  SerialAPIStateHandler();
}

void
zaf_event_distributor_app_serial_timeout(void)
{
  SerialAPIStateHandler();
}

void
PopCallBackQueue(void)
{
  if (callbackQueue.requestCnt)
  {
    callbackQueue.requestCnt--;
    if (++callbackQueue.requestOut >= MAX_CALLBACK_QUEUE)
    {
      callbackQueue.requestOut = 0;
    }
  }
  else
  {
    callbackQueue.requestOut = callbackQueue.requestIn;
  }
  retry = 0;
  set_state_and_notify(stateIdle);
}


void
PopCommandQueue(void)
{
  if (commandQueue.requestCnt)
  {
    commandQueue.requestCnt--;
    if (++commandQueue.requestOut >= MAX_UNSOLICITED_QUEUE)
    {
      commandQueue.requestOut = 0;
    }
  }
  else
  {
    commandQueue.requestOut = commandQueue.requestIn;
  }
  retry = 0;
  set_state_and_notify(stateIdle);
}

/**
 * @brief wakeup after sleep timeout event
 *
 * @param pTimer Timer connected to this method
 */
void
ZCB_WakeupTimeout(__attribute__((unused)) SSwTimer *pTimer)
{
  DPRINT("ZCB_WakeupTimeout\n");
}

/*==============================   ApplicationInitSW   ======================
**    Initialization of the Application Software
**
**--------------------------------------------------------------------------*/
void
ApplicationInitSW(void)
{
  SAppNodeInfo_t *AppNodeInfo;
  SRadioConfig_t *RadioConfig;

  AppNodeInfo = zaf_get_app_node_info();
  RadioConfig = zaf_get_radio_config();

  comm_interface_init();

  // FIXME load any saved node configuration and prepare to feed it to protocol
/* Do we together with the bTxStatus uint8_t also transmit a sTxStatusReport struct on ZW_SendData callback to HOST */
#if SUPPORT_SEND_DATA_TIMING
  bTxStatusReportEnabled = true;
#else
  bTxStatusReportEnabled = false;
#endif

#if SUPPORT_SERIAL_API_STARTUP_NOTIFICATION
  /* ZW->HOST: bWakeupReason | bWatchdogStarted | deviceOptionMask | */
  /*           nodeType_generic | nodeType_specific | cmdClassLength | cmdClass[] */

  // FIXME send startup notification via serial port if we are supposed to
  SCommandClassList_t *const apCCLists[3] =
  {
    &CommandClasses.UnSecureIncludedCC,
    &CommandClasses.SecureIncludedUnSecureCC,
    &CommandClasses.SecureIncludedSecureCC
  };

  compl_workbuf[0] = g_eApplResetReason;
#if SUPPORT_ZW_WATCHDOG_START || SUPPORT_ZW_WATCHDOG_STOP
  compl_workbuf[1] = bWatchdogStarted;
#else
  compl_workbuf[1] = false;
#endif
  compl_workbuf[2] = AppNodeInfo->DeviceOptionsMask;
  compl_workbuf[3] = AppNodeInfo->NodeType.generic;
  compl_workbuf[4] = AppNodeInfo->NodeType.specific;
  compl_workbuf[5] = apCCLists[0]->iListLength;
  uint8_t i = 0;
  if (0 < apCCLists[0]->iListLength)
  {
    for (i = 0; i < apCCLists[0]->iListLength; i++)
    {
      compl_workbuf[6 + i] = apCCLists[0]->pCommandClasses[i];
    }
  }
  eSerialAPIStartedCapabilities capabilities = (RadioConfig->eRegion == REGION_US_LR) ? SERIAL_API_STARTED_CAPABILITIES_L0NG_RANGE : 0;
  compl_workbuf[6 + i] = capabilities;

  Request(FUNC_ID_SERIAL_API_STARTED, compl_workbuf, 7 + i);

#endif /* #if SUPPORT_STARTUP_NOTIFICATION */
   AppTimerDeepSleepPersistentRegister(&mWakeupTimer, false, ZCB_WakeupTimeout);  // register for event jobs timeout event
}

#ifdef USB_SUSPEND_SUPPORT
void UsbSuspendCallback(void)
{
  set_state(stateAppSuspend);
}
#endif

/*==============================   ApplicationInit   ======================
**    Init UART and setup port pins for LEDs
**
**--------------------------------------------------------------------------*/
ZW_APPLICATION_STATUS
ApplicationInit(
  zpal_reset_reason_t eResetReason)
{
  // Serial API can control hardware with information
  // set in the file system therefore it should be the first
  // step in the Initialization
  appFileSystemInit();

  app_hw_init();

#ifdef USB_SUSPEND_SUPPORT
  SerialAPI_set_usb_supend_callback(UsbSuspendCallback);
#endif

#if SUPPORT_SERIAL_API_READY
	if (ZPAL_RESET_REASON_SLEEP == eResetReason)
	{
	  /* We have been waken from sleep by timer or external pin event - we must assume we are connected. */
	  serialLinkState = SERIAL_LINK_CONNECTED;
	}
	else
	{
		/* We have been waken either by ZPAL_RESET_REASON_POWER_ON or  ZPAL_RESET_REASON_PIN or similar. Initially we are DETACHED */
	  serialLinkState = SERIAL_LINK_CONNECTED;
	}
#endif
  /* g_eApplResetReason now contains lastest System Reset reason */
  g_eApplResetReason = eResetReason;

#ifdef DEBUGPRINT
  static uint8_t m_aDebugPrintBuffer[96];
  DebugPrintConfig(m_aDebugPrintBuffer, sizeof(m_aDebugPrintBuffer), zpal_debug_output);
  DebugPrintf("ApplicationInit eResetReason = %d\n", eResetReason);
  ZAF_PrintAppInfo();
#endif

  /*************************************************************************************
   * CREATE USER TASKS  -  ZW_ApplicationRegisterTask() and ZW_UserTask_CreateTask()
   *************************************************************************************
   * Register the main APP task function.
   *
   * ATTENTION: This function is the only task that can call ZAF API functions!!!
   * Failure to follow guidelines will result in undefined behavior.
   *
   * Furthermore, this function is the only way to register Event Notification
   * Bit Numbers for associating to given event handlers.
   *
   * ZW_UserTask_CreateTask() can be used to create additional tasks.
   * @see zwave_soc_sensor_pir example for more info.
   *************************************************************************************/
  __attribute__((unused)) bool bWasTaskCreated = ZW_ApplicationRegisterTask(
                                                    ApplicationTask,
                                                    EAPPLICATIONEVENT_ZWRX,
                                                    EAPPLICATIONEVENT_ZWCOMMANDSTATUS,
                                                    zaf_get_protocol_config()
                                                    );
  assert(bWasTaskCreated);

  return (APPLICATION_RUNNING); /*Return false to enter production test mode*/
}


#ifndef ZW_CONTROLLER_BRIDGE
/*==========================   ApplicationCommandHandler   ==================
**    Handling of received application commands and requests
**
**--------------------------------------------------------------------------*/
void /*RET Nothing                  */
ApplicationCommandHandler(__attribute__((unused)) void *pSubscriberContext, SZwaveReceivePackage* pRxPackage)
{
  ZW_APPLICATION_TX_BUFFER *pCmd = (ZW_APPLICATION_TX_BUFFER *)&pRxPackage->uReceiveParams.Rx.Payload;
  uint8_t cmdLength = pRxPackage->uReceiveParams.Rx.iLength;
  RECEIVE_OPTIONS_TYPE *rxOpt = &pRxPackage->uReceiveParams.Rx.RxOptions;
  /* ZW->PC: REQ | 0x04 | rxStatus | sourceNode | cmdLength | pCmd[] | rssiVal | securityKey */
  uint8_t offset = 0;
  BYTE_IN_AR(compl_workbuf, 0) = rxOpt->rxStatus;
  if (SERIAL_API_SETUP_NODEID_BASE_TYPE_16_BIT == nodeIdBaseType)
  {
    BYTE_IN_AR(compl_workbuf, 1) = (uint8_t)(rxOpt->sourceNode >> 8);     // MSB
    BYTE_IN_AR(compl_workbuf, 2) = (uint8_t)(rxOpt->sourceNode & 0xFF);   // LSB
    offset++;  // 16 bit nodeID means the command fields that follow are offset by one byte
  }
  else
  {
    BYTE_IN_AR(compl_workbuf, 1) = (uint8_t)(rxOpt->sourceNode & 0xFF);       // Legacy 8 bit nodeID
  }
#if defined(ZW_CONTROLLER) && !defined(ZW_CONTROLLER_STATIC)
  if (cmdLength > (uint8_t)(BUF_SIZE_TX - (offset + (rxOpt->rxStatus & RECEIVE_STATUS_FOREIGN_FRAME ? 5 : 4))))
  {
    cmdLength = (uint8_t)(BUF_SIZE_TX - (offset + (rxOpt->rxStatus & RECEIVE_STATUS_FOREIGN_FRAME ? 5 : 4)));
  }
#else
  if (cmdLength > (uint8_t)(BUF_SIZE_TX - (offset + 5)))
  {
    cmdLength = (uint8_t)(BUF_SIZE_TX - (offset + 5));
  }
#endif
  BYTE_IN_AR(compl_workbuf, offset + 2) = cmdLength;
  for (uint8_t i = 0; i < cmdLength; i++)
  {
    BYTE_IN_AR(compl_workbuf, offset + 3 + i) = *((uint8_t*)pCmd + i);
  }
  /* Syntax when a promiscuous frame is received (i.e. RECEIVE_STATUS_FOREIGN_FRAME is set): */
  /* ZW->PC: REQ | 0xD1 | rxStatus | sourceNode | cmdLength | pCmd[] | destNode | rssiVal
   * | securityKey | bSourceTxPower | bSourceNoiseFloor */
#if defined(ZW_CONTROLLER) && !defined(ZW_CONTROLLER_STATIC)
  /* For libraries supporting promiscuous mode... */
  BYTE_IN_AR(compl_workbuf, offset + 3 + cmdLength) = (uint8_t)(rxOpt->destNode & 0xFF);
  uint8_t index = (uint8_t)(offset + 3 + ((rxOpt->rxStatus & RECEIVE_STATUS_FOREIGN_FRAME) ? 1 : 0) + cmdLength);
  BYTE_IN_AR(compl_workbuf, index++) = (uint8_t)rxOpt->rxRSSIVal;
  BYTE_IN_AR(compl_workbuf, index++) = rxOpt->securityKey;
  BYTE_IN_AR(compl_workbuf, index++) = (uint8_t)rxOpt->bSourceTxPower;
  BYTE_IN_AR(compl_workbuf, index) = (uint8_t)rxOpt->bSourceNoiseFloor;
  RequestUnsolicited((rxOpt->rxStatus & RECEIVE_STATUS_FOREIGN_FRAME ?
                        FUNC_ID_PROMISCUOUS_APPLICATION_COMMAND_HANDLER : FUNC_ID_APPLICATION_COMMAND_HANDLER),
                     compl_workbuf,
                     index);
#else
  BYTE_IN_AR(compl_workbuf, offset + 3 + cmdLength) = (uint8_t)rxOpt->rxRSSIVal;
  BYTE_IN_AR(compl_workbuf, offset + 4 + cmdLength) = rxOpt->securityKey;
  BYTE_IN_AR(compl_workbuf, offset + 5 + cmdLength) = (uint8_t)rxOpt->bSourceTxPower;
  BYTE_IN_AR(compl_workbuf, offset + 6 + cmdLength) = (uint8_t)rxOpt->bSourceNoiseFloor;

  /* Less code space-consuming version for libraries without promiscuous support */
  RequestUnsolicited(FUNC_ID_APPLICATION_COMMAND_HANDLER, compl_workbuf, (uint8_t)(offset + 7 + cmdLength));
#endif
}
#endif

#ifdef ZW_CONTROLLER_BRIDGE

// Struct describing Multicast nodemask header for SerialAPI
typedef struct SMultiCastNodeMaskHeaderSerial
{
  uint8_t iNodemaskLength : 5; // Bits 0-4 is length. Length of Nodemask in bytes - Valid values [0-29]
  uint8_t iNodeMaskOffset : 3; // Bits 5-7 is offset. Denotes which node the first bit in the nodemask describes
                               // First node in nodemask is (Value * 32) + 1 - e.g. 2 -> first node is 65
                               // In reality - we always give a full nodemask -> length 29, offset 0.
} SMultiCastNodeMaskHeaderSerial;

/*======================   ApplicationCommandHandler_Bridge   ================
**    Handling of received application commands and requests
**
**--------------------------------------------------------------------------*/
static void                       /*RET Nothing                  */
ApplicationCommandHandler_Bridge(SReceiveMulti* pReceiveMulti)
{
  /* ZW->HOST: REQ | 0xA8 | rxStatus | destNode | sourceNode | cmdLength
   *          | pCmd[] | multiDestsOffset_NodeMaskLen | multiDestsNodeMask[] | rssiVal
   *          | securityKey | bSourceTxPower | bSourceNoiseFloor */
  uint8_t offset = 0;
  BYTE_IN_AR(compl_workbuf, 0) = pReceiveMulti->RxOptions.rxStatus;
  if (SERIAL_API_SETUP_NODEID_BASE_TYPE_16_BIT == nodeIdBaseType)
  {
    BYTE_IN_AR(compl_workbuf, 1) = (uint8_t)(pReceiveMulti->RxOptions.destNode >> 8);      // MSB
    BYTE_IN_AR(compl_workbuf, 2) = (uint8_t)(pReceiveMulti->RxOptions.destNode & 0xFF);    // LSB
    BYTE_IN_AR(compl_workbuf, 3) = (uint8_t)(pReceiveMulti->RxOptions.sourceNode >> 8);    // MSB
    BYTE_IN_AR(compl_workbuf, 4) = (uint8_t)(pReceiveMulti->RxOptions.sourceNode & 0xFF);  // LSB
    offset = 6;  // 16 bit nodeIDs means the command fields that follow are offset by two bytes
  }
  else
  {
    // Legacy 8 bit nodeIDs
    BYTE_IN_AR(compl_workbuf, 1) = (uint8_t)pReceiveMulti->RxOptions.destNode;
    BYTE_IN_AR(compl_workbuf, 2) = (uint8_t)pReceiveMulti->RxOptions.sourceNode;
    offset = 4;
  }

  uint32_t cmdLength = pReceiveMulti->iCommandLength;
  uint8_t i;

  if (cmdLength > sizeof(pReceiveMulti->Payload))
  {
    cmdLength = sizeof(pReceiveMulti->Payload);
  }
  if (cmdLength > (uint8_t)(BUF_SIZE_TX - offset) )
  {
    cmdLength = (uint8_t)(BUF_SIZE_TX - offset) ;
  }
  BYTE_IN_AR(compl_workbuf, offset - 1 ) = (uint8_t)cmdLength;

  memcpy(compl_workbuf + offset, (uint8_t*)&pReceiveMulti->Payload, cmdLength);

  if (pReceiveMulti->RxOptions.rxStatus & RECEIVE_STATUS_TYPE_MULTI)
  {
    /* Its a Multicast frame */

    // Create NodeMaskHeader to comply with SerialAPI
    const SMultiCastNodeMaskHeaderSerial NodeMaskHeader = {
        .iNodemaskLength = 29, // Always offset full nodemask. Hardwired to 29 (and not
                               // nodemask define) since SerialAPI is not supposed to change.
        .iNodeMaskOffset = 0   // Always full nodemask -> No offset
    };

    i = NodeMaskHeader.iNodemaskLength + 1; // + 1 for node mask headers own size.
    if (i > (uint8_t)(BUF_SIZE_TX - (offset + cmdLength)))
    {
      i = (uint8_t)(BUF_SIZE_TX - (offset + cmdLength + 1));
    }
    if (i > 0)
    {
      *(compl_workbuf + offset + cmdLength) = i - 1;
      memcpy(compl_workbuf + offset + 1 + cmdLength, (uint8_t*)pReceiveMulti->NodeMask, i - 1); // +- 1 as we already copied node mask header
      i += (uint8_t)cmdLength;
    }
  }
  else
  {
    if (cmdLength >= (uint8_t)(BUF_SIZE_TX - offset) )
    {
      cmdLength = (uint8_t)(BUF_SIZE_TX - offset -1);
      i = (uint8_t)cmdLength;
    }
    else
    {
      i = (uint8_t)(cmdLength + 1);
    }
    BYTE_IN_AR(compl_workbuf, (uint8_t)(offset + cmdLength)) = 0;
  }
  BYTE_IN_AR(compl_workbuf, offset + i) = (uint8_t)pReceiveMulti->RxOptions.rxRSSIVal;
  if (SERIAL_API_SETUP_NODEID_BASE_TYPE_16_BIT == nodeIdBaseType)
  {
    BYTE_IN_AR(compl_workbuf, offset + ++i) = pReceiveMulti->RxOptions.securityKey; //inclusion fails without this
    BYTE_IN_AR(compl_workbuf, offset + ++i) = (uint8_t)pReceiveMulti->RxOptions.bSourceTxPower;
    BYTE_IN_AR(compl_workbuf, offset + ++i) = (uint8_t)pReceiveMulti->RxOptions.bSourceNoiseFloor;
  }
  /* Unified Application Command Handler for Bridge and Virtual nodes */
  RequestUnsolicited(FUNC_ID_APPLICATION_COMMAND_HANDLER_BRIDGE, compl_workbuf, (uint8_t)(offset + 1 + i));
}
#endif


/*======================   ApplicationNodeUpdate   =========================
**    Inform the static controller/slave of node information received
**
**--------------------------------------------------------------------------*/
void                                /* RET  Nothing                         */
ApplicationNodeUpdate(
  uint8_t bStatus,                     /* IN   Status of learn mode            */
  uint16_t nodeID,                    /* IN   Node id of node sending nodeinfo*/
  uint8_t *pCmd,                       /* IN   Pointer to appl. node info      */
  uint8_t bLen                         /* IN   Node info length                */
)
{
  uint8_t offset = 0;
  BYTE_IN_AR(compl_workbuf, 0) = bStatus;
  if (SERIAL_API_SETUP_NODEID_BASE_TYPE_16_BIT == nodeIdBaseType)
  {
    BYTE_IN_AR(compl_workbuf, 1) = (uint8_t)(nodeID >> 8);     // MSB
    BYTE_IN_AR(compl_workbuf, 2) = (uint8_t)(nodeID & 0xFF);   // LSB
    offset++;  // 16 bit nodeID means the command fields that follow are offset by one byte
  }
  else
  {
    BYTE_IN_AR(compl_workbuf, 1) = (uint8_t)(nodeID & 0xFF);      // Legacy 8 bit nodeID
  }

  /*  - Buffer boundary check */
  bLen = (bLen > MAX_NODE_INFO_LENGTH) ? MAX_NODE_INFO_LENGTH : bLen;
  bLen = (bLen > (uint8_t)(BUF_SIZE_TX - (offset + 3))) ? (uint8_t)(BUF_SIZE_TX - (offset + 3)) : bLen;
  
  BYTE_IN_AR(compl_workbuf, offset + 2) = bLen;
  if(bLen > 0 && pCmd)
  {
    for (uint8_t i = 0; i < bLen; i++)
    {
      BYTE_IN_AR(compl_workbuf, offset + 3 + i) = *(pCmd + i);
    }
  }
  RequestUnsolicited(FUNC_ID_ZW_APPLICATION_UPDATE, compl_workbuf, (uint8_t)(offset + bLen + 3));
}

ZW_WEAK const void * SerialAPI_get_uart_config_ext(void)
{
  return NULL;
}

#ifdef USB_SUSPEND_SUPPORT

ZW_WEAK void SerialAPI_set_usb_supend_callback(__attribute__((unused)) SerialAPI_hw_usb_suspend_callback_t callback)
{
}

ZW_WEAK void SerialAPI_hw_usb_suspend_handler(void)
{

}

#endif
