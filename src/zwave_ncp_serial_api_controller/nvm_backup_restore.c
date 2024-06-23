/**
 * @file nvm_backup_restore.c
 * @copyright 2022 Silicon Laboratories Inc.
 */

#include <string.h>
#include <app.h>
#include <nvm_backup_restore.h>
#include <utils.h>
#include <ZW_controller_api.h>
#include <serialapi_file.h>
#include <zpal_watchdog.h>
#include <zpal_nvm.h>

//#define DEBUGPRINT
#include "DebugPrint.h"

/*WARNING: The backup/restore feature is based on the thesis that the NVM area is one continuous block even if it consist of two blocks,
 A protocol and an application block. These blocks are defined in the linker script. The blocks are addressed using the data structure below.
 The definition of the NVM blocks should not be changed, changing the definition will result in breaking the backup/restore feature*/

#define WORK_BUFFER_SIZE    64

/* HOST->ZW:
operation          [open=0|read=1|write=2|close=3]
length             desired length of read/write operation
offset(MSB)        pointer to NVM memory
offset(LSB)
buffer[]           buffer only sent for operation=write
*/
/* ZW->HOST:
retVal             [OK=0|error=1|EOF=-1]
length             actual length of read/written data
offset(MSB)        pointer to NVM memory (EOF ptr for operation=open)
offset(LSB)
buffer[]           buffer only returned for operation=read
*/


/* Macro and definitions used to get index of the different fields in NVM backup & restore buffer. */
#define NVMBACKUP_RX_SUB_CMD_IDX          (0)   /** index of sub command field in rx buffer. */
#define NVMBACKUP_RX_DATA_LEN_IDX         (1)   /** index of data length field in rx buffer. */
#define NVMBACKUP_RX_ADDR_IDX             (2)   /** index of address field in rx buffer. */
/** macro used the get index of the data in NVM Backup Restore received buffer.
@param size size of the address field.*/
#define NVMBACKUP_RX_DATA_IDX(size)       (NVMBACKUP_RX_ADDR_IDX + (size))

#define NVMBACKUP_TX_STATUS_IDX           (0)   /** index of command status field in tx buffer. */
#define NVMBACKUP_TX_DATA_LEN_IDX         (1)   /** index of data length field in tx buffer. */
#define NVMBACKUP_TX_ADDR_IDX             (2)   /** index of address field in tx buffer. */
/** macro used to get index of the data in NVM Backup Restore send buffer.
@param size size of the address field.*/
#define NVMBACKUP_TX_DATA_IDX(size)       (NVMBACKUP_TX_ADDR_IDX + (size))

#define NVMBACKUP_STATUS_SIZE             (1)  /** size of command status. */
#define NVMBACKUP_DATA_LEN_SIZE           (1)  /** size of Data length field. */


static eNVMBackupRestoreOperation NVMBackupRestoreOperationInProgress = NVMBackupRestoreOperationClose;

/**
 * This function is used to extract the address from a received frame (on serial API).
 * This address is extracted depending on the size of the address field (which depend on the command type:
 * 2 bytes for the legacy NVM_BACKUP_RESTORE, 4bytes for the new NVM_EXT_BACKUP_RESTORE).
 *
 * @param pAddr[in] pointer on the first address byte in the rx buffer.
 * @param addrSize[in] size of the address field (in bytes).
 *
 * @return NVM address
 */
static uint32_t NvmBackupAddrGet(uint8_t* pAddr, const nvm_backup_restore_addr_size_t addrSize)
{
  uint32_t address = 0;

  /* Build the address starting from the most significant byte.
  For each new byte, shift the address then add the new byte.*/
  for (uint8_t i = 0; i < addrSize; i++)
  {
    address = (address << 8) + *pAddr;
    pAddr++;
  }

  return address;
}

/**
 * This function is used to fill the address in a frame (on serial API).
 * This address is set depending on the size of the address field (which depend on the command type:
 * 2 bytes for the legacy NVM_BACKUP_RESTORE, 4bytes for the new EXT_NVM_BACKUP_RESTORE).
 *
 * @param pAddr[in] pointer on the first address byte in the tx buffer.
 * @param addrSize[in] size of the address field (in bytes).
 * @param address[in] address to fill in the tx frame
 */
static void NvmBackupAddrSet(uint8_t* pAddr, const nvm_backup_restore_addr_size_t addrSize, uint32_t address)
{
  int8_t i = 0; //must be signed to avoid infinite loop.

  /*fill the buffer starting from the lowest significant byte.
  addrSize can be 2 or 4, so there can't be any sign issue with the cast.*/
  for (i = (int8_t)addrSize-1; i >= 0; i--)
  {
    pAddr[i] = address & 0x000000FF;
    address >>= 8;
  }
}


/**
 * Must be called to open the backup restore feature
 * The function will shut down the RF, Z-Wave timer system , close the NVM system, and disable the watchdog timer
 *
 * @return true if backup/resotre is opened else false
 */
static bool NvmBackupOpen(void)
{
  SZwaveCommandPackage nvmOpen = {
       .eCommandType = EZWAVECOMMANDTYPE_NVM_BACKUP_OPEN,
       .uCommandParams.NvmBackupRestore.offset = 0,
       .uCommandParams.NvmBackupRestore.length = 0,
  };
  zpal_enable_watchdog(false);
  uint8_t bReturn = QueueProtocolCommand((uint8_t*)&nvmOpen);
  if (EQUEUENOTIFYING_STATUS_SUCCESS == bReturn)
  {
    SZwaveCommandStatusPackage cmdStatus = { .eStatusType = EZWAVECOMMANDSTATUS_NVM_BACKUP_RESTORE};
    if ((GetCommandResponse(&cmdStatus, cmdStatus.eStatusType))
        && (cmdStatus.Content.NvmBackupRestoreStatus.status))
    {
      return true;
    }
  }
  return false;
}

/**
 * Read data from the NVM area
 *
 * @param offset[in] The offset of the NVM area to read from
 * @param length[in] the length of the NVM area to read
 * @param pNvmData[out] the data read from the NVM area
 *
 * @return true if data is read else false
 */
static uint8_t NvmBackupRead( uint32_t offset, uint8_t length, uint8_t* pNvmData)
{
  SZwaveCommandPackage nvmRead = {
       .eCommandType = EZWAVECOMMANDTYPE_NVM_BACKUP_READ,
       .uCommandParams.NvmBackupRestore.offset = offset,
       .uCommandParams.NvmBackupRestore.length = length,
       .uCommandParams.NvmBackupRestore.nvmData = pNvmData
  };
  DPRINTF("NVM_Read_ 0x%08x, 0x%08x, 0x%08x\r\n",offset, length, (uint32_t)pNvmData);
  uint8_t bReturn = QueueProtocolCommand((uint8_t*)&nvmRead);
  if (EQUEUENOTIFYING_STATUS_SUCCESS == bReturn)
  {
    SZwaveCommandStatusPackage cmdStatus = { 0 };
    if (GetCommandResponse(&cmdStatus, EZWAVECOMMANDSTATUS_NVM_BACKUP_RESTORE))
    {
      if (cmdStatus.Content.NvmBackupRestoreStatus.status)
      {
    	DPRINT("NVM_READ_OK\r\n");
        return true;
      }
    }
  }
  DPRINT("NVM_READ_ERR\r\n");
  return false;
}

/**
 * Close the open/restore feature
 *
 * @return true if backup/retore feature is closed else false
 */

static uint8_t NvmBackupClose(void)
{
  SZwaveCommandPackage nvmClose = {
       .eCommandType = EZWAVECOMMANDTYPE_NVM_BACKUP_CLOSE
  };
  uint8_t bReturn = QueueProtocolCommand((uint8_t*)&nvmClose);

  return ((EQUEUENOTIFYING_STATUS_SUCCESS == bReturn)? true: false);
}

/**
 * Restore the NVM data
 *
 * @param offset[in] The offset of the NVM area to wite backup data to
 * @param length[in] the length of the backup data
 * @param pNvmData[out] the data to be written to the NVM
 *
 * @return true if data is written else false
 */
static uint8_t NvmBackupRestore( uint32_t offset, uint8_t length, uint8_t* pNvmData)
{
  SZwaveCommandPackage nvmWrite = {
       .eCommandType = EZWAVECOMMANDTYPE_NVM_BACKUP_WRITE,
       .uCommandParams.NvmBackupRestore.offset = offset,
       .uCommandParams.NvmBackupRestore.length = length,
       .uCommandParams.NvmBackupRestore.nvmData = pNvmData
  };
  uint8_t bReturn = QueueProtocolCommand((uint8_t*)&nvmWrite);
  if (EQUEUENOTIFYING_STATUS_SUCCESS == bReturn)
  {
    SZwaveCommandStatusPackage cmdStatus = { 0 };
    if (GetCommandResponse(&cmdStatus, EZWAVECOMMANDSTATUS_NVM_BACKUP_RESTORE))
    {
      if (cmdStatus.Content.NvmBackupRestoreStatus.status)
      {
    	DPRINT("NVM_WRITE_OK\r\n");
        return true;
      }
    }
  }
  DPRINT("NVM_WRITE_ERR\r\n");
  return false;
}

bool NvmBackupLegacyCmdAvailable(void)
{
  /* If NVM size is 0x10000, the legacy command should be forbidden. However, for backward
  compatibility, a controller with exactly 0x10000 bytes of NVM must be able to use it.
  WARNING: in that case, the legacy NVM backup & restore command will return a size equal to 0. */
  if (0x10000 < zpal_nvm_backup_get_size())
  {
    return false;
  }
  else
  {
    return true;
  }
}

void func_id_serial_api_nvm_backup_restore(__attribute__((unused)) uint8_t inputLength, uint8_t *pInputBuffer, uint8_t *pOutputBuffer, uint8_t *pOutputLength, bool extended)
{
  uint32_t NVM_WorkPtr = 0;
  uint8_t dataLength;
  const uint32_t nvm_storage_size = zpal_nvm_backup_get_size();
  nvm_backup_restore_addr_size_t addrSize;

  //set address size according to the command (legacy or extended backup & restore)
  if (true == extended)
  {
    addrSize = NVM_EXT_BACKUP_RESTORE_ADDR_SIZE;
  }
  else
  {
    addrSize = NVM_BACKUP_RESTORE_ADDR_SIZE;
  }

  dataLength = 0;                                   /* Assume nothing is read or written */
  memset(pOutputBuffer, 0, NVMBACKUP_STATUS_SIZE + NVMBACKUP_DATA_LEN_SIZE + addrSize);
  pOutputBuffer[NVMBACKUP_TX_STATUS_IDX] = NVMBackupRestoreReturnValueOK; /* Assume not at EOF and no ERROR */

  switch (pInputBuffer[NVMBACKUP_RX_SUB_CMD_IDX]) /* operation */
  {
    case NVMBackupRestoreOperationOpen: /* open */
    {
      if (NVMBackupRestoreOperationClose == NVMBackupRestoreOperationInProgress)
      {
        /* Lock everyone else out from making changes to the NVM content */
        /* Remember to have some kind of dead-mans-pedal to release lock again. */
        /* TODO */
        // here we have to  shut down RF and disable power management and close NVM subsystem
        if (NvmBackupOpen())
        {
          NVMBackupRestoreOperationInProgress = NVMBackupRestoreOperationOpen;
          /* Set the size of the backup/restore. (Number of bytes in flash used for file systems) */
          /* Please note that the special case where nvm_storage_size == 0x10000 is indicated by 0x00 0x00 */
          NvmBackupAddrSet( &(pOutputBuffer[NVMBACKUP_TX_ADDR_IDX]), addrSize, nvm_storage_size);

          /* in case of extended command, set the sub commands capability*/
          if (true == extended)
          {
            pOutputBuffer[NVMBACKUP_TX_DATA_IDX(addrSize) + (dataLength++)] =
                                      (1 << NVMBackupRestoreOperationOpen) +
                                      (1 << NVMBackupRestoreOperationRead) +
                                      (1 << NVMBackupRestoreOperationWrite) +
                                      (1 << NVMBackupRestoreOperationClose);
          }
          pOutputBuffer[NVMBACKUP_TX_DATA_LEN_IDX] = dataLength;
        }
        else
        {
          pOutputBuffer[NVMBACKUP_TX_STATUS_IDX] = NVMBackupRestoreReturnValueError; /*Report error we can't open backup restore feature*/
        }
      }
    }
    break;

    case NVMBackupRestoreOperationRead: /* read */
    {
      DPRINT("NVM_Read_ \r\n");
      /* Check that NVM is ready */
      if ((NVMBackupRestoreOperationInProgress != NVMBackupRestoreOperationRead) &&
          (NVMBackupRestoreOperationInProgress != NVMBackupRestoreOperationOpen))
      {
        DPRINT("NVM_Read_Mis \r\n");
        pOutputBuffer[NVMBACKUP_TX_STATUS_IDX] = NVMBackupRestoreReturnValueOperationMismatch;
        break;
      }
      NVMBackupRestoreOperationInProgress = NVMBackupRestoreOperationRead;
      /* Load input */
      dataLength = pInputBuffer[NVMBACKUP_RX_DATA_LEN_IDX]; /* Requested dataLength */
      NVM_WorkPtr = NvmBackupAddrGet( &(pInputBuffer[NVMBACKUP_RX_ADDR_IDX]), addrSize);
      /* Validate Input */
      if (dataLength > WORK_BUFFER_SIZE)/* Make sure that length isn't larger than the available buffer size */
      {
        dataLength = WORK_BUFFER_SIZE;
      }
      if ((NVM_WorkPtr + dataLength) >= nvm_storage_size)/* Make sure that we don't go beyond valid NVM content */
      {
        DPRINT("NVM_Read_EOF \r\n");
        dataLength = (uint8_t)(nvm_storage_size - NVM_WorkPtr);
        pOutputBuffer[NVMBACKUP_TX_STATUS_IDX] = (uint8_t)NVMBackupRestoreReturnValueEOF; /* Indicate at EOF */
      }
      /* fill output buffer */
      pOutputBuffer[NVMBACKUP_TX_DATA_LEN_IDX] = dataLength;
      NvmBackupAddrSet( &(pOutputBuffer[NVMBACKUP_TX_ADDR_IDX]), addrSize, NVM_WorkPtr);
      NvmBackupRead(NVM_WorkPtr, dataLength, &pOutputBuffer[NVMBACKUP_TX_DATA_IDX(addrSize)]);
    }
    break;

    case NVMBackupRestoreOperationWrite: /* write */
    {
      /* Check that NVM is ready */
      if ((NVMBackupRestoreOperationInProgress != NVMBackupRestoreOperationWrite) &&
          (NVMBackupRestoreOperationInProgress != NVMBackupRestoreOperationOpen))
      {
        DPRINT("NVM_Write_mis \r\n");
        pOutputBuffer[NVMBACKUP_TX_STATUS_IDX] = NVMBackupRestoreReturnValueOperationMismatch;
        break;
      }
      NVMBackupRestoreOperationInProgress = NVMBackupRestoreOperationWrite;
      /* Load input */
      dataLength = pInputBuffer[NVMBACKUP_RX_DATA_LEN_IDX]; /* Requested dataLength */
      NVM_WorkPtr = NvmBackupAddrGet( &(pInputBuffer[NVMBACKUP_RX_ADDR_IDX]), addrSize);
      /* Validate input */
      if (dataLength > WORK_BUFFER_SIZE)
      {
        DPRINT("NVM_Write_buff_err \r\n");
        pOutputBuffer[NVMBACKUP_TX_STATUS_IDX] = NVMBackupRestoreReturnValueError; /* ERROR: ignore request if length is larger than available buffer */
      }
      else
      {
        /* Make sure that we don't go beyond valid NVM content */
        uint8_t tmp_buf[WORK_BUFFER_SIZE];
        if ((NVM_WorkPtr + dataLength) >= nvm_storage_size)
        {
          DPRINT("NVM_Write_EOF \r\n");
          dataLength = (uint8_t)(nvm_storage_size - NVM_WorkPtr);
          pOutputBuffer[NVMBACKUP_TX_STATUS_IDX] = (uint8_t)NVMBackupRestoreReturnValueEOF; /* Indicate at EOF */
        }
        /* copy data into another buffer because write operation will be done in another task. */
        memcpy(tmp_buf, (uint8_t*)&pInputBuffer[NVMBACKUP_RX_DATA_IDX(addrSize)], dataLength);
        NvmBackupRestore(NVM_WorkPtr , dataLength, tmp_buf);
        /* fill output buffer */
        pOutputBuffer[NVMBACKUP_TX_DATA_LEN_IDX] = dataLength;
        NvmBackupAddrSet( &(pOutputBuffer[NVMBACKUP_TX_ADDR_IDX]), addrSize, NVM_WorkPtr);

      }
      /* reset data length because there is no data in output buffer. */
      dataLength = 0;
    }
    break;

    case NVMBackupRestoreOperationClose: /* close */
    {
      /* Unlock NVM content, so everyone else can make changes again */
      // here we have to  shut down RF and disable power management
      /* TODO */
      if (NVMBackupRestoreOperationInProgress == NVMBackupRestoreOperationClose)
      {
        break;
      }
      if (NvmBackupClose())
      {
        NVMBackupRestoreOperationInProgress = NVMBackupRestoreOperationClose;
      }
      else
      {
        pOutputBuffer[NVMBACKUP_TX_STATUS_IDX] = NVMBackupRestoreReturnValueError; /*report error we canot close backup restore feature*/
      }
    }
    break;

    default:
      pOutputBuffer[NVMBACKUP_TX_STATUS_IDX] = NVMBackupRestoreReturnValueError;
    break;
  }
  //build output buffer length
  *pOutputLength = (uint8_t)(NVMBACKUP_STATUS_SIZE + NVMBACKUP_DATA_LEN_SIZE + addrSize + dataLength);
}
