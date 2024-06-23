/**
 * @file
 * @copyright 2022 Silicon Laboratories Inc.
 */
#include <stdint.h>

#include <NodeMask.h>

#include "cmd_handlers.h"
#include "app.h"
#include "zaf_config.h"
#include "zw_version_config.h"
#include "nvm_backup_restore.h"

#define CAPABILITIES_SIZE (8 + 32) // Info + supported commands

/* Serial API application manufacturer_id */
#define SERIALAPI_MANUFACTURER_ID1           (uint8_t)((ZAF_CONFIG_MANUFACTURER_ID & 0xFF00) >> 8) /* MSB */
#define SERIALAPI_MANUFACTURER_ID2           (uint8_t)( ZAF_CONFIG_MANUFACTURER_ID & 0x00FF)       /* LSB */
/* Serial API application manufacturer product type */
#define SERIALAPI_MANUFACTURER_PRODUCT_TYPE1 (uint8_t)((ZAF_CONFIG_PRODUCT_TYPE_ID & 0xFF00) >> 8)     /* MSB */
#define SERIALAPI_MANUFACTURER_PRODUCT_TYPE2 (uint8_t) (ZAF_CONFIG_PRODUCT_TYPE_ID & 0x00FF)           /* LSB */
/* Serial API application manufacturer product id */
#define SERIALAPI_MANUFACTURER_PRODUCT_ID1   (uint8_t)((ZAF_CONFIG_PRODUCT_ID & 0xFF00) >> 8)      /* MSB */
#define SERIALAPI_MANUFACTURER_PRODUCT_ID2   (uint8_t) (ZAF_CONFIG_PRODUCT_ID & 0x00FF)            /* LSB */

static uint8_t SERIALAPI_CAPABILITIES[CAPABILITIES_SIZE] = {
    APP_VERSION,
    APP_REVISION,
    SERIALAPI_MANUFACTURER_ID1,
    SERIALAPI_MANUFACTURER_ID2,
    SERIALAPI_MANUFACTURER_PRODUCT_TYPE1,
    SERIALAPI_MANUFACTURER_PRODUCT_TYPE2,
    SERIALAPI_MANUFACTURER_PRODUCT_ID1,
    SERIALAPI_MANUFACTURER_PRODUCT_ID2
};

static bool add_cmd_to_capabilities(cmd_handler_map_t const * const p_cmd_entry, cmd_context_t context)
{
  ZW_NodeMaskSetBit(context, p_cmd_entry->cmd);
  return false;
}

ZW_ADD_CMD(FUNC_ID_SERIAL_API_GET_CAPABILITIES)
{
  cmd_foreach(add_cmd_to_capabilities, &SERIALAPI_CAPABILITIES[8]);

#if SUPPORT_NVM_BACKUP_RESTORE
  //If the legacy NVM backup & restore command cannot be used, it must be removed from available command.
  if (false == NvmBackupLegacyCmdAvailable())
  {
    /*SUPPORT_NVM_BACKUP_RESTORE cannot be set depending on the NVM size, because it is define in
    the application. However the NVM_SIZE is defined inside the ZPAL. It is available to the
    application only through the function zpal_nvm_backup_get_size().
    So this configuration must be done dynamically. */
    ZW_NodeMaskClearBit(&SERIALAPI_CAPABILITIES[8], FUNC_ID_NVM_BACKUP_RESTORE);
  }
#endif
  /* HOST->ZW: no params defined */
  /* ZW->HOST: RES | 0x07 | */
  /*  SERIAL_APPL_VERSION | SERIAL_APPL_REVISION | SERIALAPI_MANUFACTURER_ID1 | SERIALAPI_MANUFACTURER_ID2 | */
  /*  SERIALAPI_MANUFACTURER_PRODUCT_TYPE1 | SERIALAPI_MANUFACTURER_PRODUCT_TYPE2 | */
  /*  SERIALAPI_MANUFACTURER_PRODUCT_ID1 | SERIALAPI_MANUFACTURER_PRODUCT_ID2 | FUNCID_SUPPORTED_BITMASK[] */
  Respond(frame->cmd, SERIALAPI_CAPABILITIES, sizeof(SERIALAPI_CAPABILITIES));
}
