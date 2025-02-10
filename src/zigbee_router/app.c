
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

#include "app/framework/include/af.h"
#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif

#ifdef SL_CATALOG_ZIGBEE_NETWORK_TEST_PRESENT
#include "network_test_config.h"
#endif //SL_CATALOG_ZIGBEE_NETWORK_TEST_PRESENT

#if (LARGE_NETWORK_TESTING == 0)
#ifndef EZSP_HOST

#include "zigbee_sleep_config.h"
#endif

#include "btl_interface.h"
#include "network-steering.h"
#include "find-and-bind-target.h"
#include "sl_cli_handles.h"

#ifdef SL_CATALOG_ZIGBEE_ZLL_COMMISSIONING_COMMON_PRESENT
#include "zll-commissioning.h"
#endif //SL_CATALOG_ZIGBEE_ZLL_COMMISSIONING_COMMON_PRESENT

#if defined(SL_CATALOG_LED0_PRESENT)
#include "sl_led.h"
#include "sl_simple_led_instances.h"
#define led_turn_on(led) sl_led_turn_on(led)
#define led_turn_off(led) sl_led_turn_off(led)
#define led_toggle(led) sl_led_toggle(led)
#define COMMISSIONING_STATUS_LED (&sl_led_led0)
#define ON_OFF_LIGHT_LED         (&sl_led_led0)
#else // !SL_CATALOG_LED0_PRESENT
#define led_turn_on(led)
#define led_turn_off(led)
#define led_toggle(led)
#endif // SL_CATALOG_LED0_PRESENT

#define LED_BLINK_PERIOD_MS      500
#define LIGHT_ENDPOINT           1
#define STEERING_DELAY_MS        15000

static sl_zigbee_event_t commissioning_led_event;
static sl_zigbee_event_t finding_and_binding_event;

//---------------
// Custom CLI Commands

typedef struct {
    uint8_t major;
    uint8_t minor;
    uint16_t patch;
} gecko_version_t;

gecko_version_t parse_version(uint32_t version){
   gecko_version_t result;

   result.major = (version >> 24) & 0xFF;
   result.minor = (version >> 16) & 0xFF;
   result.patch = version & 0xFFFF;

   return result;
}

void bootloader_info(sl_cli_command_arg_t *arguments)
{
  char version_str[16];
  BootloaderInformation_t info = { .type = SL_BOOTLOADER, .version = 0L, .capabilities = 0L };

  bootloader_getInfo(&info);
  printf("%s Bootloader", info.type == SL_BOOTLOADER ? "Gecko" : "Legacy" );

  gecko_version_t ver = parse_version(info.version);
  snprintf(version_str, sizeof(version_str),
          "v%u.%02u.%02u", ver.major, ver.minor, ver.patch);
  printf(" %s\n", version_str);

}

void bootloader_reboot(sl_cli_command_arg_t *arguments)
{
  bootloader_rebootAndInstall();
}

static const sl_cli_command_info_t cmd__btl_info = \
  SL_CLI_COMMAND(bootloader_info,
                 "Bootloader info",
                 "none",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cmd__btl_reboot = \
  SL_CLI_COMMAND(bootloader_reboot,
                 "Enter bootloader",
                 "none",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_entry_t btl_group_table[] = {
    { "info", &cmd__btl_info, false },
    { "reboot", &cmd__btl_reboot, false },
    { NULL, NULL, false },
};

sl_cli_command_info_t cli_cmd_btl_group = \
  SL_CLI_COMMAND_GROUP(btl_group_table, "Bootloader commands");

// Create root command table
const sl_cli_command_entry_t sl_cli_btl_command_table[] = {
  { "bootloader", &cli_cmd_btl_group, false },
  { NULL, NULL, false },
};

sl_cli_command_group_t sl_cli_btl_command_group =
{
  { NULL },
  false,
  sl_cli_btl_command_table
};

//---------------
// Custom helper functions
static void sync_on_off_cluster(uint8_t endpoint){
  bool onOff;
  if (emberAfReadServerAttribute(endpoint,
                                ZCL_ON_OFF_CLUSTER_ID,
                                ZCL_ON_OFF_ATTRIBUTE_ID,
                                (uint8_t *)&onOff,
                                sizeof(onOff))
      == EMBER_ZCL_STATUS_SUCCESS) {
    if (onOff) {
      led_turn_on(ON_OFF_LIGHT_LED);
    } else {
      led_turn_off(ON_OFF_LIGHT_LED);
    }
  }
}

//---------------
// Event handlers

static void commissioning_led_event_handler(sl_zigbee_event_t *event)
{
  if (emberAfNetworkState() == EMBER_JOINED_NETWORK) {
    uint16_t identifyTime;
    emberAfReadServerAttribute(LIGHT_ENDPOINT,
                                ZCL_IDENTIFY_CLUSTER_ID,
                                ZCL_IDENTIFY_TIME_ATTRIBUTE_ID,
                                (uint8_t *)&identifyTime,
                                sizeof(identifyTime));
    if (identifyTime > 0 && identifyTime < 60) {
      led_toggle(COMMISSIONING_STATUS_LED);
      sl_zigbee_event_set_delay_ms(&commissioning_led_event,
                                      LED_BLINK_PERIOD_MS << 1);
    } else {
      sync_on_off_cluster(LIGHT_ENDPOINT);
    }
  } else {
    EmberStatus status = emberAfPluginNetworkSteeringStart();
    sl_zigbee_app_debug_println("%s network %s: 0x%X", "Join", "start", status);
  }
}

static void finding_and_binding_event_handler(sl_zigbee_event_t *event)
{
  if (emberAfNetworkState() == EMBER_JOINED_NETWORK) {
    sl_zigbee_event_set_inactive(&finding_and_binding_event);

    sl_zigbee_app_debug_println("Find and bind target start: 0x%X",
                                emberAfPluginFindAndBindTargetStart(LIGHT_ENDPOINT));
  }
}

//----------------------
// Implemented Callbacks

/** @brief Stack Status
 *
 * This function is called by the application framework from the stack status
 * handler.  This callbacks provides applications an opportunity to be notified
 * of changes to the stack status and take appropriate action. The framework
 * will always process the stack status after the callback returns.
 */
void emberAfStackStatusCallback(EmberStatus status)
{
  // Note, the ZLL state is automatically updated by the stack and the plugin.
  if (status == EMBER_NETWORK_DOWN) {
    led_turn_off(COMMISSIONING_STATUS_LED);
    sl_zigbee_event_set_active(&commissioning_led_event);
  } else if (status == EMBER_NETWORK_UP) {
    led_turn_on(COMMISSIONING_STATUS_LED);

    // Send announcement when coming online so coordinator detects without waiting for ping
    emberSendDeviceAnnouncement();
    sl_zigbee_event_set_active(&finding_and_binding_event);
  }
}

/** @brief Init
 * Application init function
 */
void emberAfMainInitCallback(void)
{
  sl_zigbee_event_init(&commissioning_led_event, commissioning_led_event_handler);
  sl_zigbee_event_init(&finding_and_binding_event, finding_and_binding_event_handler);

  sl_zigbee_event_set_active(&commissioning_led_event);

  // Custom bootloader CLI commands
  sl_cli_command_add_command_group(sl_cli_bootloader_handle, &sl_cli_btl_command_group);
}

/** @brief Complete network steering.
 *
 * This callback is fired when the Network Steering plugin is complete.
 *
 * @param status On success this will be set to EMBER_SUCCESS to indicate a
 * network was joined successfully. On failure this will be the status code of
 * the last join or scan attempt.
 *
 * @param totalBeacons The total number of 802.15.4 beacons that were heard,
 * including beacons from different devices with the same PAN ID.
 *
 * @param joinAttempts The number of join attempts that were made to get onto
 * an open Zigbee network.
 *
 * @param finalState The finishing state of the network steering process. From
 * this, one is able to tell on which channel mask and with which key the
 * process was complete.
 */
void emberAfPluginNetworkSteeringCompleteCallback(EmberStatus status,
                                               uint8_t totalBeacons,
                                               uint8_t joinAttempts,
                                               uint8_t finalState)
{
  sl_zigbee_app_debug_println("Join network complete: 0x%X", status);

  if (status != EMBER_SUCCESS) {
#ifdef SL_CATALOG_ZIGBEE_ZLL_COMMISSIONING_COMMON_PRESENT
    // Initialize our ZLL security now so that we are ready to be a touchlink
    // target at any point.
     status = emberAfZllSetInitialSecurityState();
    if (status != EMBER_SUCCESS) {
      sl_zigbee_app_debug_println("Error: cannot initialize ZLL security: 0x%X", status);
    }
#endif //SL_CATALOG_ZIGBEE_ZLL_COMMISSIONING_COMMON_PRESENT

    sl_zigbee_event_set_delay_ms(&commissioning_led_event,
                                    STEERING_DELAY_MS);
  }
}


/** @brief Post Attribute Change
 *
 * This function is called by the application framework after it changes an
 * attribute value. The value passed into this callback is the value to which
 * the attribute was set by the framework.
 */
void emberAfPostAttributeChangeCallback(uint8_t endpoint,
                                           EmberAfClusterId clusterId,
                                           EmberAfClusterId attributeId,
                                           uint8_t mask,
                                           uint16_t manufacturerCode,
                                           uint8_t type,
                                           uint8_t size,
                                           uint8_t* value)
{
  if (clusterId == ZCL_ON_OFF_CLUSTER_ID
      && attributeId == ZCL_ON_OFF_ATTRIBUTE_ID
      && mask == CLUSTER_MASK_SERVER) {
    sync_on_off_cluster(endpoint);
  }
}
/** @brief Identify Start Feedback
 *
 * @param endpoint The identifying endpoint Ver.: always
 * @param identifyTime The identify time Ver.: always
 */
void emberAfPluginIdentifyStartFeedbackCallback(uint8_t endpoint,
                                             uint16_t identifyTime)
{
  sl_zigbee_app_debug_println("Identify start: endpoint=%d, time=%d",
                              endpoint, identifyTime);
  if (identifyTime > 0 && identifyTime < 60) {
    sl_zigbee_app_debug_println("Identify run: endpoint=%d, time=%d",
                              endpoint, identifyTime);
    sl_zigbee_event_set_delay_ms(&commissioning_led_event,
                                  LED_BLINK_PERIOD_MS);
  }
}

void emberAfPluginIdentifyStopFeedbackCallback(uint8_t endpoint)
{
  sl_zigbee_app_debug_println("Identify stop: endpoint=%d", endpoint);
}
/** @brief On/off Cluster Server Post Init
 *
 * Following resolution of the On/Off state at startup for this endpoint, perform any
 * additional initialization needed; e.g., synchronize hardware state.
 *
 * @param endpoint Endpoint that is being initialized
 */
void emberAfPluginOnOffClusterServerPostInitCallback(uint8_t endpoint)
{
  // At startup, trigger a read of the attribute and possibly a toggle of the
  // LED to make sure they are always in sync.
  emberAfPostAttributeChangeCallback(endpoint,
                                        ZCL_ON_OFF_CLUSTER_ID,
                                        ZCL_ON_OFF_ATTRIBUTE_ID,
                                        CLUSTER_MASK_SERVER,
                                        0,
                                        0,
                                        0,
                                        NULL);
}

/** @brief
 *
 * Application framework equivalent of ::sl_zigbee_radio_needs_calibrating_handler
 */
void emberAfRadioNeedsCalibratingCallback(void)
{
  #ifndef EZSP_HOST
  sl_mac_calibrate_current_channel();
  #endif
}

#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT) && (SL_ZIGBEE_FRAMEWORK_USE_BUTTON_TO_STAY_AWAKE == 0)
#include "sl_simple_button.h"
#include "sl_simple_button_instances.h"
#ifdef SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
#include "force-sleep-wakeup.h"
#endif //SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
/***************************************************************************//**
 * A callback called in interrupt context whenever a button changes its state.
 *
 * @remark Can be implemented by the application if required. This function
 * can contain the functionality to be executed in response to changes of state
 * in each of the buttons, or callbacks to appropriate functionality.
 *
 * @note The button state should not be updated in this function, it is updated
 * by specific button driver prior to arriving here
 *
   @param[out] handle             Pointer to button instance
 ******************************************************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_RELEASED) {
    sl_zigbee_event_set_active(&finding_and_binding_event);
    #ifdef SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
    sl_zigbee_app_framework_force_wakeup();
    #endif //SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
  }
}
#endif // SL_CATALOG_SIMPLE_BUTTON_PRESENT && EMBERAPP_FRAMEWORK_USE_BUTTON_TO_STAY_AWAKE == 0

#ifdef SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
void sli_zigbee_app_framework_force_sleep_callback(void)
{
  // Do other things like turn off LEDs etc
  sl_led_turn_off(&sl_led_led0);
}
#endif // SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
#endif //#if (LARGE_NETWORK_TESTING == 0)
