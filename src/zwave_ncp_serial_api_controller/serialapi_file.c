/**
 * @file serialapi_file.c
 * @copyright 2022 Silicon Laboratories Inc.
 * @brief Z-Wave Switch On/Off Sample Application
 * @details This sample application is a Z-Wave slave node which has an LED (D2
 * on ZDP03A) that can be turned on or off from another Z-Wave node by sending
 * a Basic Set On or a Basic Set Off command.
 *
 * It can be included and excluded from a Z-Wave network by pressing S1 switch
 * on the ZDP03A board 3 times. S2 switch toggles LED D2. S3 switch transmits
 * a Node Information Frame (NIF).
 * @author Someone who started this sample application at some point in time.
 * @author Thomas Roll (TRO)
 * @author Christian Salmony Olsen (COLSEN)
 */

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/

#include <serialapi_file.h>
#include <string.h>
#include "SizeOf.h"
#include <assert.h>
//#define DEBUGPRINT
//#include "DebugPrint.h"
#include "SyncEvent.h"
#include <ZW_system_startup_api.h>
#include <ZW_application_transport_interface.h>
#include <app.h>
#include <ZAF_file_ids.h>
#include <zpal_nvm.h>
#include <zpal_misc.h>
#include <ZAF_nvm_app.h>
#include <ZAF_nvm.h>
#include "zw_version_config.h"

#define APPLICATIONSIZE (4*1024)

#define APPL_DATA_FILE_SIZE            512

#define APP_VERSION_7_15_3             0x00070F03  // 7.15.3 (NO_20DBM_SUPPORT)
#define APP_VERSION_7_18_1             0x00071201  /* 7.18.1 - The changes include the capability to set tx power to
                                                    * 20+ dBm over the serial link. */
#define APP_VERSION_7_21_0             0x00071500  /* 7.21.0 - [FIX] save the node ID type. */

// Used by the application data file.
typedef struct SApplicationData
{
  uint8_t extNvm[APPL_DATA_FILE_SIZE];
} SApplicationData;
  /* listening | generic | specific | parmLength | nodeParms[] */
typedef struct SApplicationSettings
{
  uint8_t listening;
  uint8_t generic;
  uint8_t specific;
} SApplicationSettings;

typedef struct SApplicationCmdClassInfo
{
  uint8_t UnSecureIncludedCCLen;
  uint8_t UnSecureIncludedCC[APPL_NODEPARM_MAX];
  uint8_t SecureIncludedUnSecureCCLen;
  uint8_t SecureIncludedUnSecureCC[APPL_NODEPARM_MAX];
  uint8_t SecureIncludedSecureCCLen;
  uint8_t SecureIncludedSecureCC[APPL_NODEPARM_MAX];

} SApplicationCmdClassInfo;

typedef struct SApplicationConfiguration_v7_15_3  // Cannot pack this (change size) as it is already in the field.
{
  zpal_radio_region_t rfRegion;
  int8_t              iTxPower;            // changed to zpal_tx_power_t { aka int16_t } in APP_VERSION_7_18_1
  int8_t              ipower0dbmMeasured;  // changed to zpal_tx_power_t { aka int16_t } in APP_VERSION_7_18_1
  uint8_t             radio_debug_enable;
  int16_t             maxTxPower;          // changed to zpal_tx_power_t { aka int16_t } in APP_VERSION_7_18_1
} SApplicationConfiguration_v7_15_3;

//declare the old structure only for the migration process.
//No variable should be declared with this type (excepted for migration process).
typedef struct __attribute__((packed)) SApplicationConfiguration_V7_18_1
{
  zpal_radio_region_t           rfRegion;
  zpal_tx_power_t               iTxPower;
  zpal_tx_power_t               ipower0dbmMeasured;
  uint8_t                       radio_debug_enable;
  zpal_tx_power_t               maxTxPower; // For LR only
}SApplicationConfiguration_V7_18_1;

typedef struct __attribute__((packed)) SApplicationConfiguration  // Must be packet as it is saved on NVM.
{
  zpal_radio_region_t           rfRegion;
  zpal_tx_power_t               iTxPower;
  zpal_tx_power_t               ipower0dbmMeasured;
  uint8_t                       radio_debug_enable;
  zpal_tx_power_t               maxTxPower; // For LR only
  eSerialAPISetupNodeIdBaseType nodeIdBaseType;
} SApplicationConfiguration;

#define FILE_SIZE_APPLICATIONDATA        (sizeof(SApplicationData))
#define FILE_SIZE_APPLICATIONSETTINGS     (sizeof(SApplicationSettings))
#define FILE_SIZE_APPLICATIONCMDINFO     (sizeof(SApplicationCmdClassInfo))
#define FILE_SIZE_APPLICATIONCONFIGURATION (sizeof(SApplicationConfiguration))

static void WriteDefault(void);

static void WriteDefaultApplicationConfiguration(void);
static bool ObjectExist(zpal_nvm_object_key_t key);

bool SerialAPI_GetZWVersion(uint32_t * appVersion)
{
  return ZAF_nvm_read(ZAF_FILE_ID_APP_VERSION, appVersion, ZAF_FILE_SIZE_APP_VERSION) == ZPAL_STATUS_OK;
}

bool SerialAPI_SetZWVersion(const uint32_t * appVersion)
{
  return ZAF_nvm_write(ZAF_FILE_ID_APP_VERSION, appVersion, sizeof(appVersion)) == ZPAL_STATUS_OK;
}

static void
SerialAPI_FileSystemMigrationManagement(void)
{
  //Read present file system version file
  uint32_t presentFilesysVersion = 0;
  uint32_t expectedFilesysVersion = 0;  // This will hold the file system version that current SW will support.

  SerialAPI_GetZWVersion(&presentFilesysVersion);

  expectedFilesysVersion = zpal_get_app_version();

  if(expectedFilesysVersion < presentFilesysVersion)
  {
    //System downgrade. Should not be allowed.
    assert(false);
  }
  else if(expectedFilesysVersion > presentFilesysVersion)  // File system upgrade needed. Initiating file system migration...
  {
    /**
     * Continuous migration until all needed migrations are performed,
     * to lift from any version to the latest file system version.
     */

    // If current version is 7.15.2 or older then update the FILE_ID_APPLICATIONCONFIGURATION file to the current format
    if ( presentFilesysVersion < APP_VERSION_7_15_3 )
    {
      // Add code for migration of file system to version APP_VERSION_7_15_3 (7.15.3).

      //Get length of legacy file
      size_t dataLen = 0;
      ZAF_nvm_app_get_object_size(FILE_ID_APPLICATIONCONFIGURATION, &dataLen);

      //Read legacy file to first members of tApplicationConfiguration
      SApplicationConfiguration_v7_15_3 tApplicationConfiguration = { .rfRegion = REGION_UNDEFINED };
      // Initialize, since zpal_nvm_read() might fail.
      ZAF_nvm_app_read(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration, dataLen);

      //Write default values to new members of tApplicationConfiguration and update the file.
      tApplicationConfiguration.radio_debug_enable = 0;
      tApplicationConfiguration.maxTxPower = 140;
      zpal_status_t status = ZAF_nvm_app_write(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration,
          sizeof(tApplicationConfiguration));
      if (ZPAL_STATUS_OK == status)
      {
        presentFilesysVersion = APP_VERSION_7_15_3;
      }
    }

    // Migrate files from file system version APP_VERSION_7_15_3 to APP_VERSION_7_18_1.
    if ( presentFilesysVersion < APP_VERSION_7_18_1 )
    {
      SApplicationConfiguration_v7_15_3 tApplicationConfiguration_v7_15_3 = { .rfRegion = REGION_UNDEFINED };
      SApplicationConfiguration_V7_18_1 tApplicationConfiguration = { .rfRegion = REGION_UNDEFINED };
      zpal_status_t status;

      status = ZAF_nvm_app_read(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration_v7_15_3,
          sizeof(tApplicationConfiguration_v7_15_3));
      if (ZPAL_STATUS_OK != status)
      {
        WriteDefaultApplicationConfiguration();
      }
      else
      {
        tApplicationConfiguration.rfRegion           = tApplicationConfiguration_v7_15_3.rfRegion;
        tApplicationConfiguration.iTxPower           = tApplicationConfiguration_v7_15_3.iTxPower;
        tApplicationConfiguration.ipower0dbmMeasured = tApplicationConfiguration_v7_15_3.ipower0dbmMeasured;
        tApplicationConfiguration.radio_debug_enable = tApplicationConfiguration_v7_15_3.radio_debug_enable;
        tApplicationConfiguration.maxTxPower         = tApplicationConfiguration_v7_15_3.maxTxPower;

        status = ZAF_nvm_app_write(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration,
            sizeof(tApplicationConfiguration));  /* Do not use FILE_SIZE_APPLICATIONCONFIGURATION in
                                                  * migration functions, instead hard-code the size as
                                                  * sizes do change with FW upgrades. */
        if (ZPAL_STATUS_OK == status)
        {
          presentFilesysVersion = APP_VERSION_7_18_1;
        }
      }

      // Lifted to version APP_VERSION_7_18_1
    }

#ifdef ZW_MIGRATION_FROM_7_20
    // Migrate files from file system version APP_VERSION_7_20_0 (same as 7_18_1) to APP_VERSION_7_21_0.
    if ( presentFilesysVersion < APP_VERSION_7_21_0 )
    {
      SApplicationConfiguration sAppCfgMigration = { .rfRegion = REGION_UNDEFINED };
      zpal_status_t status;

      //cannot migrate if the file system is older than V7.18.1. Other migration script should have been called before.
      assert(APP_VERSION_7_18_1 <= presentFilesysVersion);

      /*New application configuration has only a new member (no change in the legacy part). So read the
      legacy structure directly in the new one, then just set value for new member.*/
      status = ZAF_nvm_app_read(FILE_ID_APPLICATIONCONFIGURATION, &sAppCfgMigration,
          sizeof(SApplicationConfiguration_V7_18_1));
      if (ZPAL_STATUS_OK != status)
      {
        WriteDefaultApplicationConfiguration();
      }
      else
      {
        //default value for new member.
        sAppCfgMigration.nodeIdBaseType = SERIAL_API_SETUP_NODEID_BASE_TYPE_DEFAULT;

        status = ZAF_nvm_app_write(FILE_ID_APPLICATIONCONFIGURATION, &sAppCfgMigration,
            sizeof(sAppCfgMigration));   /* Do not use FILE_SIZE_APPLICATIONCONFIGURATION in
                                          * migration functions, instead hard-code the size as
                                          * sizes do change with FW upgrades. */
        if (ZPAL_STATUS_OK == status)
        {
          presentFilesysVersion = APP_VERSION_7_21_0;
        }
      }
    }
#endif /* ZW_MIGRATION_FROM_7_20 */

    /*
     * If this fails, some of the migrations were not performed due to earlier migrations that have failed.
     */
    assert(APP_VERSION_7_21_0 <= presentFilesysVersion);

    /**
     * @attention This implementation assumes that the build is going to update the ZAF_FILE_ID_APP_VERSION to the current!
     */

    /**
     * Write the new file system version number to NMV.
     */
    SerialAPI_SetZWVersion(&expectedFilesysVersion);
  }
}

uint8_t SerialApiFileInit(void)
{
  // Init application filesystem
  bool initStatus = ZAF_nvm_app_init();
  if (!initStatus)
  {
    assert(false); //Assert has been kept for debugging , can be removed from production code. This error can only be caused by some internal flash HW failure
  }

  initStatus = ZAF_nvm_init();
  if (!initStatus)
  {
    assert(false); //Assert has been kept for debugging , can be removed from production code. This error can only be caused by some internal flash HW failure
  }

  uint32_t appVersion = 0;
  bool status = SerialAPI_GetZWVersion(&appVersion);

  if (status)
  {
    if (zpal_get_app_version() != appVersion)
    {
      /**
       * In case the file-system is older than supported by this version of the FW, then upgrade.
       */
      SerialAPI_FileSystemMigrationManagement();
    }
  }
  else
  {
	  //There are no files on first boot up. Write default files.
    WriteDefault();
    return false;
  }

  return true;
}

static
bool ObjectExist(zpal_nvm_object_key_t key)
{
  zpal_status_t status;
  size_t   tDataLen;

  status = ZAF_nvm_app_get_object_size(key, &tDataLen);
  if (ZPAL_STATUS_OK != status)
  {
    return false;
  }
  return true;
}

/**
 * @brief Reads application data from file system.
 */
uint8_t SerialApiNvmReadAppData(uint32_t offset, uint8_t* pAppData, uint32_t iLength)
{
  SApplicationData tApplicationData = { 0 };

  uint8_t dataIsRead = false;
  zpal_status_t status;

  if (ObjectExist(FILE_ID_APPLICATIONDATA))
  {
    status = ZAF_nvm_app_read(FILE_ID_APPLICATIONDATA, &tApplicationData, FILE_SIZE_APPLICATIONDATA);
    if (ZPAL_STATUS_OK == status)
    {
      dataIsRead = true;
      for (uint32_t i = 0; i < iLength; i++)
      {
        pAppData[i] = tApplicationData.extNvm[i+  offset];
      }
    }
  }
  return dataIsRead;

}

/**
 * @brief Writes application data to file system.
 */
uint8_t SerialApiNvmWriteAppData(uint32_t offset, const uint8_t* pAppData, uint32_t iLength)
{
  SApplicationData tApplicationData = { 0 };
  uint8_t dataIsWritten = false;
  zpal_status_t status;

  status = ZAF_nvm_app_read(FILE_ID_APPLICATIONDATA, &tApplicationData, FILE_SIZE_APPLICATIONDATA);
  if (ZPAL_STATUS_OK == status)
  {
    memcpy((uint8_t *)&tApplicationData + offset, pAppData, iLength);
    status = ZAF_nvm_app_write(FILE_ID_APPLICATIONDATA, &tApplicationData, FILE_SIZE_APPLICATIONDATA);
    if (ZPAL_STATUS_OK == status)
    {
      dataIsWritten = true;
    }
  }
  return dataIsWritten;
}

uint8_t
SaveApplicationSettings(uint8_t bListening,
                        uint8_t bGeneric,
                        uint8_t bSpecific
                       )
{
  SApplicationSettings tApplicationSettings = { 0 };
  uint8_t dataIsWritten = false;
  zpal_status_t status;

  status = ZAF_nvm_app_read(FILE_ID_APPLICATIONSETTINGS, &tApplicationSettings, FILE_SIZE_APPLICATIONSETTINGS);
  if (ZPAL_STATUS_OK == status)
  {
    tApplicationSettings.listening = bListening;
    tApplicationSettings.generic = bGeneric;
    tApplicationSettings.specific = bSpecific;
    status = ZAF_nvm_app_write(FILE_ID_APPLICATIONSETTINGS, &tApplicationSettings, FILE_SIZE_APPLICATIONSETTINGS);
    if (ZPAL_STATUS_OK == status)
    {
      dataIsWritten = true;
    }
  }
  return dataIsWritten;
}


uint8_t
ReadApplicationSettings(uint8_t* pListening,
                        uint8_t* pGeneric,
                        uint8_t* pSpecific
                       )
{
  SApplicationSettings tApplicationSettings = { 0 };
  uint8_t dataIsRead = false;
  zpal_status_t status;

  if (ObjectExist(FILE_ID_APPLICATIONSETTINGS))
  {
    status = ZAF_nvm_app_read(FILE_ID_APPLICATIONSETTINGS, &tApplicationSettings, FILE_SIZE_APPLICATIONSETTINGS);
    if (ZPAL_STATUS_OK == status)
    {
      *pListening = tApplicationSettings.listening;
      *pGeneric = tApplicationSettings.generic ;
      *pSpecific = tApplicationSettings.specific;
      dataIsRead = true;
    }
  }
  return dataIsRead;

}


uint8_t
SaveApplicationCCInfo (uint8_t        bUnSecureIncludedCCLen,
                       const uint8_t* pUnSecureIncludedCC,
                       uint8_t        bSecureIncludedUnSecureCCLen,
                       const uint8_t* pSecureIncludedUnSecureCC,
                       uint8_t        bSecureIncludedSecureCCLen,
                       const uint8_t* pSecureIncludedSecureCC)
{
  SApplicationCmdClassInfo tApplicationCmdClassInfo = { 0 };
  uint8_t dataIsWritten = false;
  zpal_status_t status;

  status = ZAF_nvm_app_read(FILE_ID_APPLICATIONCMDINFO, &tApplicationCmdClassInfo, FILE_SIZE_APPLICATIONCMDINFO);
  if (ZPAL_STATUS_OK == status)
  {

    tApplicationCmdClassInfo.UnSecureIncludedCCLen = bUnSecureIncludedCCLen;
    tApplicationCmdClassInfo.SecureIncludedUnSecureCCLen = bSecureIncludedUnSecureCCLen;
    tApplicationCmdClassInfo.SecureIncludedSecureCCLen = bSecureIncludedSecureCCLen;

    for (uint8_t i = 0; i < APPL_NODEPARM_MAX; i++)
    {
      if (i < bUnSecureIncludedCCLen)
      {
        tApplicationCmdClassInfo.UnSecureIncludedCC[i] = pUnSecureIncludedCC[i];
      }
      else
      {
        tApplicationCmdClassInfo.UnSecureIncludedCC[i] = 0;
      }

      if (i < bSecureIncludedUnSecureCCLen)
      {
        tApplicationCmdClassInfo.SecureIncludedUnSecureCC[i] = pSecureIncludedUnSecureCC[i];
      }
      else
      {
        tApplicationCmdClassInfo.SecureIncludedUnSecureCC[i] = 0;
      }

      if (i < bSecureIncludedSecureCCLen)
      {
        tApplicationCmdClassInfo.SecureIncludedSecureCC[i] = pSecureIncludedSecureCC[i];
      }
      else
      {
        tApplicationCmdClassInfo.SecureIncludedSecureCC[i] = 0;
      }

      }
      status = ZAF_nvm_app_write(FILE_ID_APPLICATIONCMDINFO, &tApplicationCmdClassInfo, FILE_SIZE_APPLICATIONCMDINFO);
      if (ZPAL_STATUS_OK == status)
      {
        dataIsWritten = true;
      }
  }
  return dataIsWritten;

}


uint8_t
ReadApplicationCCInfo (uint8_t* pUnSecureIncludedCCLen,
                       uint8_t* pUnSecureIncludedCC,
                       uint8_t* pSecureIncludedUnSecureCCLen,
                       uint8_t* pSecureIncludedUnSecureCC,
                       uint8_t* pSecureIncludedSecureCCLen,
                       uint8_t* pSecureIncludedSecureCC)
{
  SApplicationCmdClassInfo tApplicationCmdClassInfo = { 0 };
  uint8_t dataIsRead = false;
  zpal_status_t status;

  if (ObjectExist(FILE_ID_APPLICATIONCMDINFO))
  {
    status = ZAF_nvm_app_read(FILE_ID_APPLICATIONCMDINFO, &tApplicationCmdClassInfo, FILE_SIZE_APPLICATIONCMDINFO);
    if (ZPAL_STATUS_OK == status)
    {
      dataIsRead = true;
      *pUnSecureIncludedCCLen = tApplicationCmdClassInfo.UnSecureIncludedCCLen;
      *pSecureIncludedUnSecureCCLen = tApplicationCmdClassInfo.SecureIncludedUnSecureCCLen;
      *pSecureIncludedSecureCCLen = tApplicationCmdClassInfo.SecureIncludedSecureCCLen;
      for (uint8_t i = 0; i < APPL_NODEPARM_MAX; i++)
      {
        if (i < *pUnSecureIncludedCCLen)
        {
          pUnSecureIncludedCC[i] = tApplicationCmdClassInfo.UnSecureIncludedCC[i] ;
        }
        else
        {
          pUnSecureIncludedCC[i] = 0;
        }

        if (i < *pSecureIncludedUnSecureCCLen)
        {
          pSecureIncludedUnSecureCC[i] = tApplicationCmdClassInfo.SecureIncludedUnSecureCC[i];
        }
        else
        {
          pSecureIncludedUnSecureCC[i] = 0;
        }

        if (i < *pSecureIncludedSecureCCLen)
        {
          pSecureIncludedSecureCC[i] = tApplicationCmdClassInfo.SecureIncludedSecureCC[i];
        }
        else
        {
          pSecureIncludedSecureCC[i] = 0;
        }

      }
    }
  }
  return dataIsRead;

}

uint8_t
SaveApplicationRfRegion(zpal_radio_region_t rfRegion)
{
  SApplicationConfiguration tApplicationConfiguration = { 0 };
  uint8_t dataIsWritten = false;
  zpal_status_t status;

  status = ZAF_nvm_app_read(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration, FILE_SIZE_APPLICATIONCONFIGURATION);
  if (ZPAL_STATUS_OK == status)
  {
    tApplicationConfiguration.rfRegion = rfRegion;
    status = ZAF_nvm_app_write(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration, FILE_SIZE_APPLICATIONCONFIGURATION);
    if (ZPAL_STATUS_OK == status)
    {
      dataIsWritten = true;
    }
  }
  return dataIsWritten;
}

uint8_t
ReadApplicationRfRegion(zpal_radio_region_t* rfRegion)
{
  SApplicationConfiguration tApplicationConfiguration = { 0 };
  uint8_t dataIsRead = false;
  zpal_status_t status;

  if (ObjectExist(FILE_ID_APPLICATIONCONFIGURATION))
  {
    status = ZAF_nvm_app_read(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration, FILE_SIZE_APPLICATIONCONFIGURATION);
    if (ZPAL_STATUS_OK == status)
    {
      *rfRegion = tApplicationConfiguration.rfRegion;
      dataIsRead = true;
    }
  }
  return dataIsRead;

}

uint8_t
SaveApplicationNodeIdBaseType(eSerialAPISetupNodeIdBaseType nodeIdBaseType)
{
  SApplicationConfiguration tApplicationConfiguration = { .rfRegion = REGION_UNDEFINED };
  uint8_t dataIsWritten = false;
  
  if (ZPAL_STATUS_OK == ZAF_nvm_app_read(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration, FILE_SIZE_APPLICATIONCONFIGURATION))
  {
    tApplicationConfiguration.nodeIdBaseType = nodeIdBaseType;
    if (ZPAL_STATUS_OK == ZAF_nvm_app_write(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration, FILE_SIZE_APPLICATIONCONFIGURATION))
    {
      dataIsWritten = true;
    }
  }
  return dataIsWritten;
}

uint8_t
ReadApplicationNodeIdBaseType(eSerialAPISetupNodeIdBaseType* nodeIdBaseType)
{
  SApplicationConfiguration tApplicationConfiguration = { .rfRegion = REGION_UNDEFINED };
  uint8_t dataIsRead = false;
  
  if (ObjectExist(FILE_ID_APPLICATIONCONFIGURATION)
    && (ZPAL_STATUS_OK == ZAF_nvm_app_read(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration, FILE_SIZE_APPLICATIONCONFIGURATION)))
  {
    *nodeIdBaseType = tApplicationConfiguration.nodeIdBaseType;
      dataIsRead = true;
  }
  return dataIsRead;

}

uint8_t
SaveApplicationTxPowerlevel(zpal_tx_power_t ipower, zpal_tx_power_t power0dbmMeasured)
{
  SApplicationConfiguration tApplicationConfiguration = { 0 };
  uint8_t dataIsWritten = false;
  zpal_status_t status;

  status = ZAF_nvm_app_read(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration, FILE_SIZE_APPLICATIONCONFIGURATION);
  if (ZPAL_STATUS_OK == status)
  {
    tApplicationConfiguration.iTxPower = ipower;
    tApplicationConfiguration.ipower0dbmMeasured = power0dbmMeasured;
    status = ZAF_nvm_app_write(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration, FILE_SIZE_APPLICATIONCONFIGURATION);
    if (ZPAL_STATUS_OK == status)
    {
      dataIsWritten = true;
    }
  }
  return dataIsWritten;
}


uint8_t
ReadApplicationTxPowerlevel(zpal_tx_power_t *ipower, zpal_tx_power_t *power0dbmMeasured)
{
  SApplicationConfiguration tApplicationConfiguration = { 0 };
  uint8_t dataIsRead = false;
  zpal_status_t status;

  if (ObjectExist(FILE_ID_APPLICATIONCONFIGURATION))
  {
    status = ZAF_nvm_app_read(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration, FILE_SIZE_APPLICATIONCONFIGURATION);
    if (ZPAL_STATUS_OK == status)
    {
      *ipower = tApplicationConfiguration.iTxPower;
      *power0dbmMeasured = tApplicationConfiguration.ipower0dbmMeasured;
      dataIsRead = true;
    }
  }
  return dataIsRead;
}


uint8_t
SaveApplicationMaxLRTxPwr(zpal_tx_power_t maxTxPwr)
{
  SApplicationConfiguration tApplicationConfiguration = { 0 };
  uint8_t dataIsWritten = false;
  zpal_status_t status;

  status = ZAF_nvm_app_read(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration, FILE_SIZE_APPLICATIONCONFIGURATION);
  if (ZPAL_STATUS_OK == status)
  {
    tApplicationConfiguration.maxTxPower = maxTxPwr;
    status = ZAF_nvm_app_write(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration, FILE_SIZE_APPLICATIONCONFIGURATION);
    if (ZPAL_STATUS_OK == status)
    {
      dataIsWritten = true;
    }
  }
  return dataIsWritten;
}


uint8_t
ReadApplicationMaxLRTxPwr(zpal_tx_power_t *maxTxPwr)
{
  SApplicationConfiguration tApplicationConfiguration = { 0 };
  uint8_t dataIsRead = false;
  zpal_status_t status;

  if (ObjectExist(FILE_ID_APPLICATIONCONFIGURATION))
  {
    status = ZAF_nvm_app_read(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration, FILE_SIZE_APPLICATIONCONFIGURATION);
    if (ZPAL_STATUS_OK == status)
    {
      *maxTxPwr = tApplicationConfiguration.maxTxPower;
      dataIsRead = true;
    }
  }
  return dataIsRead;
}


uint8_t
SaveApplicationEnablePTI(uint8_t radio_debug_enable)
{
  SApplicationConfiguration tApplicationConfiguration = { 0 };
  uint8_t dataIsWritten = false;
  zpal_status_t status;

  status = ZAF_nvm_app_read(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration, FILE_SIZE_APPLICATIONCONFIGURATION);
  if (ZPAL_STATUS_OK == status)
  {
    tApplicationConfiguration.radio_debug_enable = radio_debug_enable;
    status = ZAF_nvm_app_write(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration, FILE_SIZE_APPLICATIONCONFIGURATION);
    if (ZPAL_STATUS_OK == status)
    {
      dataIsWritten = true;
    }
  }
  return dataIsWritten;
}


uint8_t
ReadApplicationEnablePTI(uint8_t *radio_debug_enable)
{
  SApplicationConfiguration tApplicationConfiguration = { 0 };
  uint8_t dataIsRead = false;
  zpal_status_t status;

  if (ObjectExist(FILE_ID_APPLICATIONCONFIGURATION))
  {
    status = ZAF_nvm_app_read(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration, FILE_SIZE_APPLICATIONCONFIGURATION);
    if (ZPAL_STATUS_OK == status)
    {
      *radio_debug_enable = tApplicationConfiguration.radio_debug_enable;
      dataIsRead = true;
    }
  }
  return dataIsRead;
}


uint32_t
ReadApplicationVersion(void)
{
  uint32_t appVersion = 0;
  SerialAPI_GetZWVersion(&appVersion);
  return appVersion;
}

static void
WriteDefaultApplicationSettings(void)
{
  //Write default Application Settings file
  SApplicationSettings tApplicationSettings = { 0 };
  ZAF_nvm_app_write(FILE_ID_APPLICATIONSETTINGS, &tApplicationSettings, sizeof(SApplicationSettings));
}

static void
WriteDefaultApplicationCmdClassInfo(void)
{
  //Write default ApplicationCmdClassInfo file
  SApplicationCmdClassInfo tApplicationCmdClassInfo = { 0 };
  ZAF_nvm_app_write(FILE_ID_APPLICATIONCMDINFO, &tApplicationCmdClassInfo, sizeof(SApplicationCmdClassInfo));
}

static void
WriteDefaultApplicationData(void)
{
  //Write default Controller Info file
  SApplicationData tApplicationData = { 0 };
  ZAF_nvm_app_write(FILE_ID_APPLICATIONDATA, &tApplicationData, sizeof(SApplicationData));
}

static void
WriteDefaultApplicationConfiguration(void)
{
  //Write default Application Configuration file
  SApplicationConfiguration tApplicationConfiguration = { 0 };
  ZAF_nvm_app_write(FILE_ID_APPLICATIONCONFIGURATION, &tApplicationConfiguration, sizeof(SApplicationConfiguration));
}

static void
WriteDefaultApplicationFileSystemVersion(void)
{
  //Write Application filesystem version
  uint32_t appVersion = (APP_VERSION << 16) | (APP_REVISION << 8) | APP_PATCH;
  SerialAPI_SetZWVersion(&appVersion);
}

static void
WriteDefault(void)
{
  WriteDefaultApplicationSettings();

  WriteDefaultApplicationCmdClassInfo();

  WriteDefaultApplicationData();

  WriteDefaultApplicationConfiguration();

  WriteDefaultApplicationFileSystemVersion();
}
