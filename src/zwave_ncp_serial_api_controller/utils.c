/**
 * @file utils.c
 * @copyright 2022 Silicon Laboratories Inc.
 */

#include <assert.h>
#include <string.h>
#include <utils.h>
#include <app.h>
#include <ZAF_Common_interface.h>

static TaskHandle_t task_handle;

uint8_t GetCommandResponse(SZwaveCommandStatusPackage *pCmdStatus, EZwaveCommandStatusType cmdType)
{
  const SApplicationHandles * m_pAppHandles = ZAF_getAppHandle();
  TaskHandle_t m_pAppTaskHandle = GetTaskHandle();
  QueueHandle_t Queue = m_pAppHandles->ZwCommandStatusQueue;
  for (uint8_t delayCount = 0; delayCount < 100; delayCount++)
  {
    for (UBaseType_t QueueElmCount = uxQueueMessagesWaiting(Queue);  QueueElmCount > 0; QueueElmCount--)
    {
      if (xQueueReceive(Queue, (uint8_t*)pCmdStatus, 0))
      {
        if (pCmdStatus->eStatusType == cmdType)
        {
          if (m_pAppTaskHandle  && (0 < uxQueueMessagesWaiting(Queue)))
          {
            /* More elements in queue call xTaskNotify */
            __attribute__((unused)) BaseType_t Status = xTaskNotify(m_pAppTaskHandle, 1 << EAPPLICATIONEVENT_ZWCOMMANDSTATUS, eSetBits);
            assert(Status == pdPASS); // We probably received a bad Task handle
          }
          return true;
        }
        else
        {
          /* Re-insert none-matching message into Queue */
          __attribute__((unused)) BaseType_t result = xQueueSendToBack(Queue, (uint8_t*)pCmdStatus, 0);
          assert(pdTRUE == result);
        }
      }
    }
    vTaskDelay(10);
  }
  if (m_pAppTaskHandle && (0 < uxQueueMessagesWaiting(Queue)))
  {
    /* Only call xTaskNotify if still elements in queue */
    __attribute__((unused)) BaseType_t Status = xTaskNotify(m_pAppTaskHandle, 1 << EAPPLICATIONEVENT_ZWCOMMANDSTATUS, eSetBits);
    assert(Status == pdPASS); // We probably received a bad Task handle
  }
  return false;
}

uint8_t IsPrimaryController(void)
{
  const SApplicationHandles *m_pAppHandles = ZAF_getAppHandle();
  SZwaveCommandPackage cmdPackage = {
    .eCommandType = EZWAVECOMMANDTYPE_IS_PRIMARY_CTRL
  };
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(m_pAppHandles->pZwCommandQueue, (uint8_t *)&cmdPackage, 500);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
  SZwaveCommandStatusPackage cmdStatus = { 0 };
  if (GetCommandResponse(&cmdStatus, EZWAVECOMMANDSTATUS_IS_PRIMARY_CTRL))
  {
    return cmdStatus.Content.IsPrimaryCtrlStatus.result;
  }
  assert(false);
  return 0;
}

uint8_t GetControllerCapabilities(void)
{
  const SApplicationHandles *m_pAppHandles = ZAF_getAppHandle();
  SZwaveCommandPackage cmdPackage = {
    .eCommandType = EZWAVECOMMANDTYPE_GET_CONTROLLER_CAPABILITIES
  };
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(m_pAppHandles->pZwCommandQueue, (uint8_t *)&cmdPackage, 500);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
  SZwaveCommandStatusPackage cmdStatus = { .eStatusType = EZWAVECOMMANDSTATUS_GET_CONTROLLER_CAPABILITIES };
  if (GetCommandResponse(&cmdStatus, cmdStatus.eStatusType))
  {
    return cmdStatus.Content.GetControllerCapabilitiesStatus.result;
  }
  assert(false);
  return 0;
}

uint8_t QueueProtocolCommand(uint8_t *pCommand)
{
  const SApplicationHandles *m_pAppHandles = ZAF_getAppHandle();
  // Put the Command on queue (and dont wait for it, queue must be empty)
  return (QueueNotifyingSendToBack(m_pAppHandles->pZwCommandQueue, pCommand, 0));
}

/**
* Aquire node information from protocol
*
* Method requires CommandStatus queue from protocol to be empty.
* Method requires CommandQueue to protocol to be empty.
* Method will cause assert on failure.
*
* @param[in]     NodeId       ID of node to get information about.
* @param[out]    pNodeInfo    Pointer to t_extNodeInfo struct where aquired node info can be stored.
*/
void GetNodeInfo(uint16_t NodeId, t_ExtNodeInfo* pNodeInfo)
{
  const SApplicationHandles *m_pAppHandles = ZAF_getAppHandle();
  SZwaveCommandPackage GetNodeInfoCommand = {
    .eCommandType = EZWAVECOMMANDTYPE_NODE_INFO,
    .uCommandParams.NodeInfo.NodeId = NodeId
  };

  // Put the Command on queue (and dont wait for it, queue must be empty)
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(m_pAppHandles->pZwCommandQueue, (uint8_t *)&GetNodeInfoCommand, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
  // Wait for protocol to handle command (it shouldnt take long)
  SZwaveCommandStatusPackage NodeInfo = { .eStatusType = EZWAVECOMMANDSTATUS_NODE_INFO};
  if (GetCommandResponse(&NodeInfo, NodeInfo.eStatusType))
  {
    if (NodeInfo.Content.NodeInfoStatus.NodeId == NodeId)
    {
      memcpy(pNodeInfo, (uint8_t*)&NodeInfo.Content.NodeInfoStatus.extNodeInfo, sizeof(NodeInfo.Content.NodeInfoStatus.extNodeInfo));
      return;
    }
  }
  assert(false);
}

/**
* Aquire a list of included nodes IDS in the network from protocol
*
* Method requires CommandStatus queue from protocol to be empty.
* Method requires CommandQueue to protocol to be empty.
* Method will cause assert on failure.
*
* @param[out]    node_id_list    Pointer to bitmask list where aquired included nodes IDs saved
*/
void Get_included_nodes(uint8_t* node_id_list)
{
  const SApplicationHandles *m_pAppHandles = ZAF_getAppHandle();
  SZwaveCommandPackage GetIncludedNodesCommand = {
      .eCommandType = EZWAVECOMMANDTYPE_ZW_GET_INCLUDED_NODES};

  // Put the Command on queue (and dont wait for it, queue must be empty)
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(m_pAppHandles->pZwCommandQueue, (uint8_t *)&GetIncludedNodesCommand, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
  // Wait for protocol to handle command (it shouldnt take long)
  SZwaveCommandStatusPackage includedNodes = { .eStatusType = EZWAVECOMMANDSTATUS_ZW_GET_INCLUDED_NODES };
  if (GetCommandResponse(&includedNodes, includedNodes.eStatusType))
  {
    memcpy(node_id_list, (uint8_t*)includedNodes.Content.GetIncludedNodes.node_id_list, sizeof(NODE_MASK_TYPE));
    return;
  }
  assert(false);
}


/**
* Acquire a list of included long range nodes IDS in the network from protocol.
*
* Method requires CommandStatus queue from protocol to be empty.
* Method requires CommandQueue to protocol to be empty.
* Method will cause assert on failure.
*
* @param[out]    node_id_list    Pointer to bitmask list where aquired included nodes IDs saved
*/
void Get_included_lr_nodes(uint8_t* node_id_list)
{
  const SApplicationHandles * m_pAppHandles = ZAF_getAppHandle();
  SZwaveCommandPackage GetIncludedNodesCommand = {
      .eCommandType = EZWAVECOMMANDTYPE_ZW_GET_INCLUDED_LR_NODES
  };

  // Put the Command on queue (and don't wait for it, queue is most likely empty)
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(m_pAppHandles->pZwCommandQueue, (uint8_t *)&GetIncludedNodesCommand, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
  // Wait for protocol to handle command (it shouldn't take long)
  SZwaveCommandStatusPackage includedNodes = { 0 };
  if (GetCommandResponse(&includedNodes, EZWAVECOMMANDSTATUS_ZW_GET_INCLUDED_LR_NODES))
  {
    memcpy(node_id_list, (uint8_t*)includedNodes.Content.GetIncludedNodesLR.node_id_list, sizeof(LR_NODE_MASK_TYPE));
    return;
  }
  assert(false);
}


void TriggerNotification(EApplicationEvent event)
{
  TaskHandle_t m_pAppTaskHandle = GetTaskHandle();
  if (m_pAppTaskHandle)
  {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xTaskNotifyFromISR(m_pAppTaskHandle,
                1 << event,
                eSetBits,
                &xHigherPriorityTaskWoken
    );
    /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context switch
    should be performed to ensure the interrupt returns directly to the highest
    priority task.  The macro used for this purpose is dependent on the port in
    use and may be called portEND_SWITCHING_ISR(). */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
  }

}

#define LR_AUTO_CHANNEL_CONFIG_MASK       0xF0
#define LR_CURRENT_ACTIVE_CHANNEL_MASK    0x0F

void GetLongRangeChannel(uint8_t * channel_n, uint8_t *auto_channel_config)
{
  const SApplicationHandles * m_pAppHandles = ZAF_getAppHandle();
  SZwaveCommandPackage cmdPackage = {
    .eCommandType = EZWAVECOMMANDTYPE_ZW_GET_LR_CHANNEL
  };
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(m_pAppHandles->pZwCommandQueue, (uint8_t *)&cmdPackage, 500);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
  SZwaveCommandStatusPackage cmdStatus = { 0 };
  *auto_channel_config = 0;
  *channel_n = 0;
  if (GetCommandResponse(&cmdStatus, EZWAVECOMMANDSTATUS_ZW_GET_LR_CHANNEL))
  {
    *channel_n = cmdStatus.Content.GetLRChannel.result & LR_CURRENT_ACTIVE_CHANNEL_MASK;
    *auto_channel_config = cmdStatus.Content.GetLRChannel.result & LR_AUTO_CHANNEL_CONFIG_MASK;
    return;
  }
  assert(false);
}

bool SetLongRangeChannel(uint8_t channel)
{
  const SApplicationHandles * m_pAppHandles = ZAF_getAppHandle();
  SZwaveCommandPackage cmdPackage = {
    .eCommandType = EZWAVECOMMANDTYPE_ZW_SET_LR_CHANNEL,
    .uCommandParams.SetLRChannel.value = channel
  };
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(m_pAppHandles->pZwCommandQueue, (uint8_t *)&cmdPackage, 500);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
  SZwaveCommandStatusPackage cmdStatus = { 0 };
  if (GetCommandResponse(&cmdStatus, EZWAVECOMMANDSTATUS_ZW_SET_LR_CHANNEL))
  {
    return cmdStatus.Content.SetLRChannel.result;
  }
  assert(false);
  return 0;

}

void SetLongRangeVirtualNodes(uint8_t bitmask)
{
  const SApplicationHandles * m_pAppHandles = ZAF_getAppHandle();
  SZwaveCommandPackage cmdPackage = {
    .eCommandType = EZWAVECOMMANDTYPE_ZW_SET_LR_VIRTUAL_IDS,
    .uCommandParams.SetLRVirtualNodeIDs.value = bitmask
  };
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(m_pAppHandles->pZwCommandQueue, (uint8_t *)&cmdPackage, 500);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
}

uint8_t GetPTIConfig(void)
{
  const SApplicationHandles * m_pAppHandles = ZAF_getAppHandle();
  SZwaveCommandPackage cmdPackage = {
    .eCommandType = EZWAVECOMMANDTYPE_ZW_GET_PTI_CONFIG
  };
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus = QueueNotifyingSendToBack(m_pAppHandles->pZwCommandQueue, (uint8_t *)&cmdPackage, 500);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
  SZwaveCommandStatusPackage cmdStatus = { .eStatusType = EZWAVECOMMANDSTATUS_ZW_GET_PTI_CONFIG };
  if (GetCommandResponse(&cmdStatus, cmdStatus.eStatusType))
  {
    return cmdStatus.Content.GetPTIconfig.result;
  }
  assert(false);
  return 0;
}

void SetTaskHandle(TaskHandle_t new_task_handle)
{
  task_handle = new_task_handle;
}

TaskHandle_t GetTaskHandle(void)
{
  return task_handle;
}
