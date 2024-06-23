/**
 * @file
 * @copyright 2022 Silicon Laboratories Inc.
 */

#ifndef APPS_SERIALAPI_NVM_BACKUP_RESTROE_H_
#define APPS_SERIALAPI_NVM_BACKUP_RESTROE_H_

#include <stdint.h>
#include "SerialAPI.h"

/**
 * This function is used to check if the legacy nvm backup & restore command can be used.
 * The legacy command cannot be used if address of the NVM are larger than 2 bytes.
 * @return true or false depending on the NVM size
 */
bool NvmBackupLegacyCmdAvailable(void);

/**
 * Must be called upon receiving a "Serial API NVM Backup/restore commands".
 * @param inputLength Length of data in input buffer.
 * @param pInputBuffer Input buffer
 * @param pOutputBuffer Output buffer
 * @param pOutputLength Length of data in output buffer.
 * @param extended Input Length of the address field
 */
void func_id_serial_api_nvm_backup_restore(uint8_t   inputLength,
                                           uint8_t* pInputBuffer,
                                           uint8_t* pOutputBuffer,
                                           uint8_t* pOutputLength,
                                           bool extended);

#endif /* APPS_SERIALAPI_NVM_BACKUP_RESTROE_H_ */
