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

#ifndef _ZW_VERSION_CONFIG_H_
#define _ZW_VERSION_CONFIG_H_

// <<< Use Configuration Wizard in Context Menu >>>

// <h> Override the default application version with USER_APP_* defines"

// <q USE_USER_APP_VERSION> false: Use Default Z-Wave version numbering, true: Use Application Version Configuration numbering
// <i> Default: 0
#define USE_USER_APP_VERSION  0

// </h>
// <h> Application Version Configuration

// <o USER_APP_VERSION> Application Major Version <1..255:1> <f.d>
// <i> Default: 1
#define USER_APP_VERSION  1

// <o USER_APP_REVISION> Application Minor Version <1..255:1> <f.d>
// <i> Default: 0
#define USER_APP_REVISION  0

// <o USER_APP_PATCH> Application Patch Version <1..255:1> <f.d>
// <i> Default: 0
#define USER_APP_PATCH  0

// </h>

// <<< end of configuration section >>>

#if USE_USER_APP_VERSION
    #define APP_VERSION USER_APP_VERSION
    #define APP_REVISION USER_APP_REVISION
    #define APP_PATCH USER_APP_PATCH
#else
    #ifdef ZW_SLAVE
        #define APP_VERSION ZAF_VERSION_MAJOR
        #define APP_REVISION ZAF_VERSION_MINOR
        #define APP_PATCH ZAF_VERSION_PATCH
    #else
        #define APP_VERSION SDK_VERSION_MAJOR
        #define APP_REVISION SDK_VERSION_MINOR
        #define APP_PATCH SDK_VERSION_PATCH
    #endif
#endif
#endif /* _ZW_VERSION_CONFIG_H_ */
