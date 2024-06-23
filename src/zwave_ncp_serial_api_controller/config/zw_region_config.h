/***************************************************************************//**
 * # License
 * <b> Copyright 2024 Silicon Laboratories Inc. www.silabs.com </b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of the Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * https://www.silabs.com/about-us/legal/master-software-license-agreement
 * By installing, copying or otherwise using this software, you agree to the
 * terms of the MSLA.
 *
 ******************************************************************************/

#ifndef _ZW_REGION_CONFIG_H_
#define _ZW_REGION_CONFIG_H_

// <<< Use Configuration Wizard in Context Menu >>>

// <h> Z-Wave Radio Configuration

// <o ZW_REGION> The supported region of the device. Used only if region value is not previously written in MFG_ZWAVE_COUNTRY_FREQ token. To delete value from MFG token, run: commander flash --tokengroup znet --token MFG_ZWAVE_COUNTRY_FREQ:0xFF
// <i> The supported region of the device. Used only if region value is not previously written in MFG_ZWAVE_COUNTRY_FREQ token. To delete value from MFG token, run: commander flash --tokengroup znet --token MFG_ZWAVE_COUNTRY_FREQ:0xFF
// <REGION_DEFAULT=> Default (Europe)
// <REGION_ANZ=> Australia / New Zealand
// <REGION_CN=> China
// <REGION_EU=> Europe
// <REGION_HK=> Hong Kong
// <REGION_IL=> Israel
// <REGION_IN=> India
// <REGION_JP=> Japan
// <REGION_KR=> Korea
// <REGION_RU=> Russia
// <REGION_US=> United States
// <REGION_US_LR=> United States Long Range
// <REGION_EU_LR=> Europe Long Range
// <i> Default: REGION_DEFAULT
#define ZW_REGION  REGION_DEFAULT

// </h>

// <<< end of configuration section >>>

#endif /* _ZW_REGION_CONFIG_H_ */
