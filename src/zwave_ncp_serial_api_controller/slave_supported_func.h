/**
 * @file
 * @copyright 2022 Silicon Laboratories Inc.
 */

#include "common_supported_func.h"

#define SUPPORT_GET_ROUTING_TABLE_LINE                  0 /* ZW_GetRoutingInfo */
#define SUPPORT_NVM_BACKUP_RESTORE                      0 /* MemoryGetBuffer */
#define SUPPORT_ZW_ADD_NODE_TO_NETWORK                  0 /* ZW_AddNodeToNetwork */
#define SUPPORT_ZW_ASSIGN_RETURN_ROUTE                  0 /* ZW_AssignReturnRoute */
#define SUPPORT_ZW_ASSIGN_PRIORITY_RETURN_ROUTE         0 /* ZW_AssignPriorityReturnRoute */
#define SUPPORT_ZW_ASSIGN_SUC_RETURN_ROUTE              0 /* ZW_AssignSUCReturnRoute */
#define SUPPORT_ZW_ASSIGN_PRIORITY_SUC_RETURN_ROUTE     0 /* ZW_AssignPrioritySUCReturnRoute */
#define SUPPORT_ZW_CONTROLLER_CHANGE                    0 /* ZW_ControllerChange */
#define SUPPORT_ZW_DELETE_RETURN_ROUTE                  0 /* ZW_DeleteReturnRoute */
#define SUPPORT_ZW_DELETE_SUC_RETURN_ROUTE              0 /* ZW_DeleteSUCReturnRoute */
#define SUPPORT_ZW_GET_CONTROLLER_CAPABILITIES          0 /* ZW_GetControllerCapabilities */
#define SUPPORT_ZW_GET_PRIORITY_ROUTE                   0 /* ZW_GetPriorityRoute */
#define SUPPORT_ZW_SET_PRIORITY_ROUTE                   0 /* ZW_SetPriorityRoute */
#define SUPPORT_ZW_IS_FAILED_NODE_ID                    0 /* ZW_isFailedNode */
#define SUPPORT_ZW_REMOVE_FAILED_NODE_ID                0 /* ZW_RemoveFailedNode */
#define SUPPORT_ZW_REMOVE_NODE_FROM_NETWORK             0 /* ZW_RemoveNodeFromNetwork */
#define SUPPORT_ZW_REMOVE_NODE_ID_FROM_NETWORK          0 /* ZW_RemoveNodeIDFromNetwork */
#define SUPPORT_ZW_REPLACE_FAILED_NODE                  0 /* ZW_ReplaceFailedNode */
#define SUPPORT_ZW_REPLICATION_COMMAND_COMPLETE         0 /* ZW_ReplicationReceiveComplete */
#define SUPPORT_ZW_REPLICATION_SEND_DATA                0 /* ZW_ReplicationSend */
#define SUPPORT_ZW_REQUEST_NODE_INFO                    0 /* ZW_RequestNodeInfo */
#define SUPPORT_ZW_REQUEST_NODE_NEIGHBOR_UPDATE         0 /* ZW_RequestNodeNeighborUpdate */
#define SUPPORT_ZW_SEND_DATA_EX                         1 /* ZW_SendDataEx */
#define SUPPORT_ZW_SEND_DATA_MULTI_EX                   1 /* ZW_SendDataMultiEx */
#define SUPPORT_ZW_GET_SECURITY_KEYS                    1 /* ZW_GetSecurityKeys */
#define SUPPORT_ZW_SEND_SUC_ID                          0 /* ZW_SendSUCID */
#define SUPPORT_ZW_SEND_TEST_FRAME                      0 /* ZW_SendTestFrame */
#define SUPPORT_ZW_SET_ROUTING_MAX                      0 /*notUsed*/
#define SUPPORT_ZW_NVR_GET_VALUE                        0 /* ZW_NVRGetValue */
#define SUPPORT_ZW_INITIATE_SHUTDOWN                    0
#define SUPPORT_SERIAL_API_GET_LR_NODES                 0
#define SUPPORT_SERIAL_GET_LR_CHANNEL                   0
#define SUPPORT_SERIAL_SET_LR_CHANNEL                   0
#define SUPPORT_SERIAL_SET_LR_VIRTUAL_IDS               0
#define SUPPORT_SERIAL_ENABLE_RADIO_PTI                 0

/* */
#define SUPPORT_SERIAL_API_GET_APPL_HOST_MEMORY_OFFSET  0

/**************************************************************************/
/* Common for all slaves */
/* SerialAPI functionality support definitions */
#define SUPPORT_ZW_APPLICATION_CONTROLLER_UPDATE        0

/**************************************************************************/
/* Slave enhanced */
/* Specific SerialAPI functionality support definitions */

#define SUPPORT_ZW_GET_VIRTUAL_NODES                    0 /* ZW_GetVirtualNodes */
#define SUPPORT_ZW_IS_VIRTUAL_NODE                      0 /* ZW_IsVirtualNode */
#define SUPPORT_ZW_SEND_DATA_BRIDGE                     0 /* ZW_SendData_Bridge */
#define SUPPORT_ZW_SEND_DATA_MULTI_BRIDGE               0 /* ZW_SendDataMulti_Bridge */
#define SUPPORT_ZW_SET_SLAVE_LEARN_MODE                 0 /* ZW_SetSlaveLearnMode */
#define SUPPORT_SERIAL_API_APPL_SLAVE_NODE_INFORMATION  0
#define SUPPORT_ZW_SEND_SLAVE_NODE_INFORMATION          0
