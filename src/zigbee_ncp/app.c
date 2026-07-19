/***************************************************************************//**
 * @file app.c
 * @brief Callbacks implementation and application specific code.
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include PLATFORM_HEADER
#include "sl_zigbee.h"

//----------------------
// Implemented Callbacks

/** @brief
 *
 * Application framework equivalent of ::sl_zigbee_radio_needs_calibrating_handler
 */
void sl_zigbee_af_radio_needs_calibrating_cb(void)
{
  sl_mac_calibrate_current_channel();
}

/** @brief Init
 * Application init function
 */
void sl_zigbee_af_main_init_cb(void)
{
}

//------------------------------------------------------------------------------
// Multicast override (XNCP_FEATURE_MEMBER_OF_ALL_GROUPS)
//------------------------------------------------------------------------------

bool __wrap_sli_zigbee_am_multicast_member(xncp_multicast_id_t multicastId)
{
    (void)multicastId;
    // Ignore all binding and multicast table logic, we want all group packets
    return true;
}
