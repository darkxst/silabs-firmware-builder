/**
 * @file
 * Header file for Serial API implementation.
 *
 * Contains various application definitions and SerialAPI
 * functionality support definitions.
 *
 * @copyright 2019 Silicon Laboratories Inc.
 */
#ifndef _SERIALAPPL_H_
#define _SERIALAPPL_H_
#include <ZW_typedefs.h>

#ifndef UNIT_TEST
/* Z-Wave library functionality support definitions */
#ifdef ZW_SLAVE
#include <slave_supported_func.h>
#else
#include <controller_supported_func.h>
#endif
#endif /*#ifndef UNIT_TEST*/

#include <SerialAPI.h>

#include <ZW.h>

#ifdef ZW_SLAVE
#include <ZW_slave_api.h>
#endif

#ifdef ZW_CONTROLLER
#include <ZW_controller_api.h>
#endif

/* Serial API version */
#define SERIAL_API_VER 10

/* Max number of times a frame will be transmitted to PC */
#define MAX_SERIAL_RETRY 3

/* Number of bytes in a homeID */
#define HOMEID_LENGTH 4

/* Max number of nodes in a multi cast (group) */
#define MAX_GROUP_NODES 64

/* Macro for accessing the byte in byte_array at the index indx */
#define BYTE_IN_AR(byte_array, indx) (*(byte_array + indx))

/* Macro for getting HIGH uint8_t in wVar uint16_t variable */
#define BYTE_GET_HIGH_BYTE_IN_WORD(wVar) *((uint8_t*)&wVar)

/* Macro for getting LOW uint8_t in wVar uint16_t variable */
#define BYTE_GET_LOW_BYTE_IN_WORD(wVar) *((uint8_t*)&wVar + 1)

/* Macro for setting HIGH uint8_t and LOW uint8_t in wVar uint16_t variable */
#define WORD_SET_HIGH_LOW_BYTES(wVar, bHIGHByte, bLOWByte) BYTE_GET_HIGH_BYTE_IN_WORD(wVar) = bHIGHByte; \
                                                           BYTE_GET_LOW_BYTE_IN_WORD(wVar)  = bLOWByte

/* States for ApplicationPoll function */
enum
{
  stateStartup,
  stateIdle,
  stateTxSerial,
  stateFrameParse,
  stateCallbackTxSerial,
  stateCommandTxSerial,
  stateAppSuspend
};

/* States for FUNC_ID_NVM_BACKUP_RESTORE operation */
typedef enum
{
  NVMBackupRestoreOperationOpen,
  NVMBackupRestoreOperationRead,
  NVMBackupRestoreOperationWrite,
  NVMBackupRestoreOperationClose
} eNVMBackupRestoreOperation;


/* Return values for FUNC_ID_NVM_BACKUP_RESTORE operation */
typedef enum
{
  NVMBackupRestoreReturnValueOK = false,                /* Everything is OK, so far */
  NVMBackupRestoreReturnValueError = true,              /* Non specific error */
  NVMBackupRestoreReturnValueOperationMismatch,         /* Error mixing read and write */
  NVMBackupRestoreReturnValueOperationDisturbed,        /* Error read operation disturbed by other write */
  NVMBackupRestoreReturnValueEOF = EOF                  /* Not really an error. Just an indication of EndOfFile */
} eNVMBackupRestoreReturnValue;

/* params used by ApplicationNodeInformation */
#define APPL_NODEPARM_MAX       35
#define APPL_SLAVENODEPARM_MAX  APPL_NODEPARM_MAX


typedef enum _E_SERIALAPI_SET_LEARN_MODE_
{
  SERIALPI_SET_LEARN_MODE_ZW_SET_LEARN_MODE_DISABLE           = ZW_SET_LEARN_MODE_DISABLE,
  SERIALPI_SET_LEARN_MODE_ZW_SET_LEARN_MODE_CLASSIC           = ZW_SET_LEARN_MODE_CLASSIC,
  SERIALPI_SET_LEARN_MODE_ZW_SET_LEARN_MODE_NWI               = ZW_SET_LEARN_MODE_NWI,
  SERIALPI_SET_LEARN_MODE_ZW_SET_LEARN_MODE_NWE               = ZW_SET_LEARN_MODE_NWE,
  SERIALPI_SET_LEARN_MODE_ZW_SET_LEARN_MODE_MAX               = SERIALPI_SET_LEARN_MODE_ZW_SET_LEARN_MODE_NWE,

  /* slave_learn_plus/ctrl_learn extensions */
  SERIALPI_SET_LEARN_MODE_LEARN_PLUS_OFFSET                   = 0x80,

} E_SERIALAPI_SET_LEARN_MODE;

#ifdef ZW_SLAVE_ROUTING
/* SerialAPI only used state - used when ZW_RequestNodeInfo transmit fails */
/* It is then assumed that the destination node did not receive the request. */
#define UPDATE_STATE_NODE_INFO_REQ_FAILED   0x81
#endif

/* SerialAPI functionality support definitions */
#define SUPPORT_SEND_DATA_TIMING                        1
/* Definitions for SerialAPI startup */
typedef enum
{
  SERIALAPI_CONFIG_STARTUP_NOTIFICATION_ENABLED = 1,
  SERIALAPI_CONFIG_UNDEFINED = 0xFE
} SERIALAPI_CONFIG_T;

#define SUPPORT_ZW_SET_SECURITY_S0_NETWORK_KEY          0  /*deprecated*/
/* Enable support for SerialAPI Startup Notification */
#define SUPPORT_SERIAL_API_STARTUP_NOTIFICATION         1

/* Security in Protocol SerialAPI functionality support definitions */
#define SUPPORT_SERIAL_API_APPL_NODE_INFORMATION_CMD_CLASSES  0
#define SUPPORT_ZW_SECURITY_SETUP                       0
#define SUPPORT_APPLICATION_SECURITY_EVENT              0

/* Common SerialAPI functionality support definitions */
#define SUPPORT_SERIAL_API_APPL_NODE_INFORMATION        1

#define SUPPORT_SERIAL_API_GET_CAPABILITIES             1
#define SUPPORT_SERIAL_API_SOFT_RESET                   1

#define SUPPORT_SERIAL_API_POWER_MANAGEMENT             0
#define SUPPORT_SERIAL_API_READY                        0

#define SUPPORT_SERIAL_API_EXT                          1
#define SUPPORT_SERIAL_API_APPL_NODE_INFORMATION_CMD_CLASSES  0

#ifdef ZW_ENABLE_RTC
#define SUPPORT_CLOCK_SET                               1
#define SUPPORT_CLOCK_GET                               1
#define SUPPORT_CLOCK_CMP                               1
#define SUPPORT_RTC_TIMER_CREATE                        1
#define SUPPORT_RTC_TIMER_READ                          1
#define SUPPORT_RTC_TIMER_DELETE                        1
#define SUPPORT_RTC_TIMER_CALL                          1
#else
#define SUPPORT_CLOCK_SET                               0
#define SUPPORT_CLOCK_GET                               0
#define SUPPORT_CLOCK_CMP                               0
#define SUPPORT_RTC_TIMER_CREATE                        0
#define SUPPORT_RTC_TIMER_READ                          0
#define SUPPORT_RTC_TIMER_DELETE                        0
#define SUPPORT_RTC_TIMER_CALL                          0
#endif

#define SUPPORT_ZW_AUTO_PROGRAMMING                     1

#ifdef TIMER_SUPPORT
#define SUPPORT_TIMER_START                             1
#define SUPPORT_TIMER_RESTART                           1
#define SUPPORT_TIMER_CANCEL                            1
#define SUPPORT_TIMER_CALL                              1
#else
#define SUPPORT_TIMER_START                             0
#define SUPPORT_TIMER_RESTART                           0
#define SUPPORT_TIMER_CANCEL                            0
#define SUPPORT_TIMER_CALL                              0
#endif

/* ZW_EnableSUC() no longer exists in the library */

/* */
#define SUPPORT_SERIAL_API_GET_APPL_HOST_MEMORY_OFFSET  0

#if SUPPORT_SERIAL_API_READY
enum
{
  /* SERIAL_LINK_IDLE = Ready for incomming Serial communication, but */
  /* do not transmit anything via the serial link even if application */
  /* frames is received on the RF, which normally should be transmitted */
  /* to the HOST. */
  SERIAL_LINK_DETACHED = 0,
  /* SERIAL_LINK_CONNECTED = There exists a HOST so transmit on serial */
  /* link if needed. */
  SERIAL_LINK_CONNECTED = 1
};

extern uint8_t serialLinkState;
#endif /* SUPPORT_SERIAL_API_READY */

extern void DoRespond_workbuf(
  uint8_t cnt);

extern void set_state_and_notify(
  uint8_t st
);

extern void set_state(
  uint8_t st
);

extern bool Request(
  uint8_t cmd,             /*IN   Command                  */
  uint8_t *pData,         /*IN   pointer to data          */
  uint8_t len              /*IN   Length of data           */
);

extern bool RequestUnsolicited(
  uint8_t cmd,             /*IN   Command                  */
  uint8_t *pData,         /*IN   pointer to data          */
  uint8_t len              /*IN   Length of data           */
);

extern void Respond(
  uint8_t cmd,             /*IN   Command                  */
  uint8_t const * pData,         /*IN   pointer to data          */
  uint8_t len              /*IN   Length of data           */
);
extern void DoRespond(uint8_t retVal);

extern void PopCallBackQueue(void);

extern void PopCommandQueue(void);

extern uint8_t GetCallbackCnt(void);

extern void ZW_GetMfgTokenDataCountryFreq(void *data);

#if SUPPORT_SERIAL_API_POWER_MANAGEMENT
extern void
ZCB_PowerManagementWakeUpOnExternalActive(void);

extern void
ZCB_PowerManagementWakeUpOnTimerHandler(void);

extern void
ZCB_powerManagementPoweredUpPinActive(void);

extern void
PowerManagementSetPowerDown(void);

extern void
PowerManagementSetPowerUp(void);

extern void
PowerManagementCheck(void);

extern void
PurgeCallbackQueue(void);

extern void
PurgeCommandQueue(void);
#endif /* SUPPORT_SERIAL_API_POWER_MANAGEMENT */

// Prioritized events that can wakeup protocol thread.
typedef enum EApplicationEvent
{
  EAPPLICATIONEVENT_ZWRX = 0,
  EAPPLICATIONEVENT_ZWCOMMANDSTATUS,
  EAPPLICATIONEVENT_STATECHANGE,
  EAPPLICATIONEVENT_SERIALDATARX,
  EAPPLICATIONEVENT_SERIALTIMEOUT,  
  EAPPLICATIONEVENT_TIMER
} EApplicationEvent;

/* FUNC_ID_SERIAL_API_STARTED Capabilities bit field definitions */
typedef enum
{
  SERIAL_API_STARTED_CAPABILITIES_L0NG_RANGE = 1<<0 // Controller is Z-Wave Long Range capable
  /* Can be extended with future capability bits here */
} eSerialAPIStartedCapabilities;

extern void ApplicationNodeUpdate(uint8_t bStatus, uint16_t nodeID, uint8_t *pCmd, uint8_t bLen);

/* Should be enough */
#define BUF_SIZE_RX 168
#define BUF_SIZE_TX 168

extern uint8_t compl_workbuf[BUF_SIZE_TX];

#endif /* _SERIALAPPL_H_ */
