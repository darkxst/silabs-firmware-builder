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

#ifndef _ZW_CONFIG_RF_H_
#define _ZW_CONFIG_RF_H_

// <<< Use Configuration Wizard in Context Menu >>>

// <h> Max Tx Power Configuration

// <o APP_MAX_TX_POWER_LR> The maximum allowed Tx power in deci dBm for Z-Wave Long Range network <140..200:1> <f.d>
// <i> The maximum allowed Tx power in deci dBm for Z-Wave Long Range network
// <i> Default: 140
#define APP_MAX_TX_POWER_LR  140

// <o APP_MAX_TX_POWER> The maximum allowed Tx power in deci dBm for Z-Wave network <-100..200:1> <f.d>
// <i> The maximum allowed Tx power in deci dBm for Z-Wave network
// <i> Default: 0
#define APP_MAX_TX_POWER  0

// <o APP_MEASURED_0DBM_TX_POWER> The deci dBmoutput measured at a PA setting of 0dBm <-50..50:1> <f.d>
// <i> The deci dBm output measured at a PA setting of 0dBm
// <i> Default: 0
#define APP_MEASURED_0DBM_TX_POWER  0

// <q ENABLE_RADIO_DEBUG> Enable Radio Debug
// <i> Enable Radio Debug
// <i> Default: 0
#define ENABLE_RADIO_DEBUG  0

// </h>

// <<< end of configuration section >>>

#endif /* _ZW_CONFIG_RF_H_ */
