/***************************************************************************//**
 * @file
 * @brief OpenThread stack configuration file.
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#ifndef _SL_OPENTHREAD_FEATURES_CONFIG_H
#define _SL_OPENTHREAD_FEATURES_CONFIG_H
//-------- <<< Use Configuration Wizard in Context Menu >>> -----------------
//
// <h> Default OpenThread Stack Configuration

// <h>  Thread Stack Protocol Version
// <o   OPENTHREAD_CONFIG_THREAD_VERSION>
//      <OT_THREAD_VERSION_1_1=> Thread 1.1
//      <OT_THREAD_VERSION_1_2=> Thread 1.2
//      <OT_THREAD_VERSION_1_3=> Thread 1.3
//      <OT_THREAD_VERSION_1_4=> Thread 1.4
// <i>  Current Default: OT_THREAD_VERSION_1_3
#ifndef OPENTHREAD_CONFIG_THREAD_VERSION
#define OPENTHREAD_CONFIG_THREAD_VERSION OT_THREAD_VERSION_1_3
#endif
// </h>

#if (OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2)
// <h>  The following features require at least Thread Stack Protocol Version 1.2
// <q>  Backbone Router
#ifndef OPENTHREAD_CONFIG_BACKBONE_ROUTER_ENABLE
#define OPENTHREAD_CONFIG_BACKBONE_ROUTER_ENABLE    0
#endif
// <q> CSL Auto Synchronization using data polling
#ifndef OPENTHREAD_CONFIG_MAC_CSL_AUTO_SYNC_ENABLE
#define OPENTHREAD_CONFIG_MAC_CSL_AUTO_SYNC_ENABLE  1
#endif
// <q>  CSL (Coordinated Sampled Listening) Debug
#ifndef OPENTHREAD_CONFIG_MAC_CSL_DEBUG_ENABLE
#define OPENTHREAD_CONFIG_MAC_CSL_DEBUG_ENABLE      0
#endif
// <q>  CSL (Coordinated Sampled Listening) Receiver
#ifndef OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
#define OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE   1
#endif
// <o SL_OPENTHREAD_CSL_TX_UNCERTAINTY> CSL Scheduling Uncertainty (±10 us units) <12..999:1>
#ifndef SL_OPENTHREAD_CSL_TX_UNCERTAINTY
#if OPENTHREAD_RADIO
  #define SL_OPENTHREAD_CSL_TX_UNCERTAINTY 175
#elif OPENTHREAD_FTD
  #define SL_OPENTHREAD_CSL_TX_UNCERTAINTY 20
#else
  #define SL_OPENTHREAD_CSL_TX_UNCERTAINTY 12
#endif
#endif
// <q>  DUA (Domain Unicast Address)
#ifndef OPENTHREAD_CONFIG_DUA_ENABLE
#define OPENTHREAD_CONFIG_DUA_ENABLE                1
#endif
// <q>  Link Metrics Initiator
#ifndef OPENTHREAD_CONFIG_MLE_LINK_METRICS_INITIATOR_ENABLE
#define OPENTHREAD_CONFIG_MLE_LINK_METRICS_INITIATOR_ENABLE 1
#endif
// <q>  Link Metrics Subject
#ifndef OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE
#define OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE 1
#endif
// <q>  Multicast Listener Registration
#ifndef OPENTHREAD_CONFIG_MLR_ENABLE
#define OPENTHREAD_CONFIG_MLR_ENABLE                1
#endif
// </h>
#endif // OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2

#if (OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_3)
// <h>  The following features require at least Thread Stack Protocol Version 1.3
// <q>  DNS Client
#ifndef OPENTHREAD_CONFIG_DNS_CLIENT_ENABLE
#define OPENTHREAD_CONFIG_DNS_CLIENT_ENABLE         1
#endif
// <q>  DNS-SD Server
#ifndef OPENTHREAD_CONFIG_DNSSD_SERVER_ENABLE
#define OPENTHREAD_CONFIG_DNSSD_SERVER_ENABLE       0
#endif
// <q>  Service Registration Protocol (SRP) Client
#ifndef OPENTHREAD_CONFIG_SRP_CLIENT_ENABLE
#define OPENTHREAD_CONFIG_SRP_CLIENT_ENABLE         1
#endif
// <q>  Service Registration Protocol (SRP) Server
#ifndef OPENTHREAD_CONFIG_SRP_SERVER_ENABLE
#define OPENTHREAD_CONFIG_SRP_SERVER_ENABLE         0
#endif
// <q>  TCPlp (Low power TCP over OpenThread)
#ifndef OPENTHREAD_CONFIG_TCP_ENABLE
#define OPENTHREAD_CONFIG_TCP_ENABLE                0
#endif
// <q>  DNS Client over TCP
#ifndef OPENTHREAD_CONFIG_DNS_CLIENT_OVER_TCP_ENABLE
#define OPENTHREAD_CONFIG_DNS_CLIENT_OVER_TCP_ENABLE 0
#endif
// <q> Thread over Infrastructure (NCP only)
#ifndef OPENTHREAD_CONFIG_RADIO_LINK_TREL_ENABLE
#define OPENTHREAD_CONFIG_RADIO_LINK_TREL_ENABLE 0
#endif
// </h>
#endif // OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_3

// <e>  Border Agent
#ifndef OPENTHREAD_CONFIG_BORDER_AGENT_ENABLE
#define OPENTHREAD_CONFIG_BORDER_AGENT_ENABLE       0
#endif
// </e>
// <e>  Border Router
#ifndef OPENTHREAD_CONFIG_BORDER_ROUTER_ENABLE
#define OPENTHREAD_CONFIG_BORDER_ROUTER_ENABLE      0
#endif
// </e>
// <e>  Channel Manager
#ifndef OPENTHREAD_CONFIG_CHANNEL_MANAGER_ENABLE
#define OPENTHREAD_CONFIG_CHANNEL_MANAGER_ENABLE    0
#endif
// </e>
// <e>  Channel Monitor
#ifndef OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
#define OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE    0
#endif
// </e>

// <e OPENTHREAD_CONFIG_COMMISSIONER_ENABLE>  Commissioner
#ifndef OPENTHREAD_CONFIG_COMMISSIONER_ENABLE
#define OPENTHREAD_CONFIG_COMMISSIONER_ENABLE       0
#endif

// <o OPENTHREAD_CONFIG_COMMISSIONER_MAX_JOINER_ENTRIES> Max Joiner Entries
// <i> The maximum number of Joiner entries maintained by the Commissioner.
// <d> 2
#ifndef OPENTHREAD_CONFIG_COMMISSIONER_MAX_JOINER_ENTRIES
#define OPENTHREAD_CONFIG_COMMISSIONER_MAX_JOINER_ENTRIES       2
#endif
// </e>

// <e>  COAP API
#ifndef OPENTHREAD_CONFIG_COAP_API_ENABLE
#define OPENTHREAD_CONFIG_COAP_API_ENABLE           0
#endif
// </e>
// <e>  COAP Observe (RFC7641) API
#ifndef OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
#define OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE   0
#endif
// </e>
// <e>  COAP Secure API
#ifndef OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
#define OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE    0
#endif
// </e>
// <e>  DHCP6 Client
#ifndef OPENTHREAD_CONFIG_DHCP6_CLIENT_ENABLE
#define OPENTHREAD_CONFIG_DHCP6_CLIENT_ENABLE       0
#endif
// </e>
// <e>  DHCP6 Server
#ifndef OPENTHREAD_CONFIG_DHCP6_SERVER_ENABLE
#define OPENTHREAD_CONFIG_DHCP6_SERVER_ENABLE       0
#endif
// </e>
// <e>  Diagnostic
#ifndef OPENTHREAD_CONFIG_DIAG_ENABLE
#define OPENTHREAD_CONFIG_DIAG_ENABLE               0
#endif
// </e>
// <e>  ECDSA (Elliptic Curve Digital Signature Algorithm) (Required for Matter support)
#ifndef OPENTHREAD_CONFIG_ECDSA_ENABLE
#define OPENTHREAD_CONFIG_ECDSA_ENABLE              1
#endif
// </e>
// <e>  External Heap
#ifndef OPENTHREAD_CONFIG_HEAP_EXTERNAL_ENABLE
#define OPENTHREAD_CONFIG_HEAP_EXTERNAL_ENABLE      0
#endif
// </e>
// <e>  IPv6 Fragmentation
#ifndef OPENTHREAD_CONFIG_IP6_FRAGMENTATION_ENABLE
#define OPENTHREAD_CONFIG_IP6_FRAGMENTATION_ENABLE  0
#endif
// </e>
// <e>  Maximum number of IPv6 unicast addresses allowed to be externally added
#ifndef OPENTHREAD_CONFIG_IP6_MAX_EXT_UCAST_ADDRS
#define OPENTHREAD_CONFIG_IP6_MAX_EXT_UCAST_ADDRS   4
#endif
// </e>
// <e>  Maximum number of IPv6 multicast addresses allowed to be externally added
#ifndef OPENTHREAD_CONFIG_IP6_MAX_EXT_MCAST_ADDRS
#define OPENTHREAD_CONFIG_IP6_MAX_EXT_MCAST_ADDRS   4
#endif
// </e>
// <e>  Jam Detection
#ifndef OPENTHREAD_CONFIG_JAM_DETECTION_ENABLE
#define OPENTHREAD_CONFIG_JAM_DETECTION_ENABLE      0
#endif
// </e>
// <e>  Joiner
#ifndef OPENTHREAD_CONFIG_JOINER_ENABLE
#define OPENTHREAD_CONFIG_JOINER_ENABLE             0
#endif
// </e>
// <e>  Link Raw Service
#ifndef OPENTHREAD_CONFIG_LINK_RAW_ENABLE
#define OPENTHREAD_CONFIG_LINK_RAW_ENABLE           0
#endif
// </e>
// <e>  MAC Filter
#ifndef OPENTHREAD_CONFIG_MAC_FILTER_ENABLE
#define OPENTHREAD_CONFIG_MAC_FILTER_ENABLE         0
#endif
// </e>
// <e>  MLE Long Routes extension (experimental)
#ifndef OPENTHREAD_CONFIG_MLE_LONG_ROUTES_ENABLE
#define OPENTHREAD_CONFIG_MLE_LONG_ROUTES_ENABLE    0
#endif
// </e>
// <e>  MultiPAN RCP
#ifndef OPENTHREAD_CONFIG_MULTIPAN_RCP_ENABLE
#define OPENTHREAD_CONFIG_MULTIPAN_RCP_ENABLE       0
#endif
// </e>
// <e>  Multiple OpenThread Instances
#ifndef OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
#define OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE      0
#endif
// </e>
// <e>  Multiple Static Instance Support
#ifndef OPENTHREAD_CONFIG_MULTIPLE_STATIC_INSTANCE_ENABLE
#define OPENTHREAD_CONFIG_MULTIPLE_STATIC_INSTANCE_ENABLE      0
#endif
// </e>
// <e>  Number of OpenThread Instances For Static Buffer Allocation
#ifndef OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_NUM
#define OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_NUM      2
#endif
// </e>
// <e>  Define broadcast IID for spinel frames dedicated to all hosts in multipan configuration
#ifndef OPENTHREAD_SPINEL_CONFIG_BROADCAST_IID
#define OPENTHREAD_SPINEL_CONFIG_BROADCAST_IID      0
#endif
// </e>
// <e>  OTNS (OpenThread Network Simulator)
#ifndef OPENTHREAD_CONFIG_OTNS_ENABLE
#define OPENTHREAD_CONFIG_OTNS_ENABLE               0
#endif
// </e>
// <e>  Ping Sender Module
#ifndef OPENTHREAD_CONFIG_PING_SENDER_ENABLE
#define OPENTHREAD_CONFIG_PING_SENDER_ENABLE        1
#endif

// </e>
// <e>  Power Calibration Module  (RCP only configuration)
#ifndef OPENTHREAD_CONFIG_POWER_CALIBRATION_ENABLE
#define OPENTHREAD_CONFIG_POWER_CALIBRATION_ENABLE  0
#endif

// </e>
// <e>  Platform UDP
#ifndef OPENTHREAD_CONFIG_PLATFORM_UDP_ENABLE
#define OPENTHREAD_CONFIG_PLATFORM_UDP_ENABLE       0
#endif
// </e>
// <e>  Reference Device for Thread Test Harness
#ifndef OPENTHREAD_CONFIG_REFERENCE_DEVICE_ENABLE
#define OPENTHREAD_CONFIG_REFERENCE_DEVICE_ENABLE   0
#endif
// </e>
// <e>  Service Entries in Thread Network Data
#ifndef OPENTHREAD_CONFIG_TMF_NETDATA_SERVICE_ENABLE
#define OPENTHREAD_CONFIG_TMF_NETDATA_SERVICE_ENABLE    0
#endif
// </e>
// <e>  RAM (volatile-only storage)
#ifndef OPENTHREAD_SETTINGS_RAM
#define OPENTHREAD_SETTINGS_RAM                     0
#endif
// </e>
// <e>  SLAAC Addresses
#ifndef OPENTHREAD_CONFIG_IP6_SLAAC_ENABLE
#define OPENTHREAD_CONFIG_IP6_SLAAC_ENABLE          1
#endif
// </e>
// <e>  SNTP Client
#ifndef OPENTHREAD_CONFIG_SNTP_CLIENT_ENABLE
#define OPENTHREAD_CONFIG_SNTP_CLIENT_ENABLE        0
#endif
// </e>
// <e>  TMF Network Diagnostic client API
#ifndef OPENTHREAD_CONFIG_TMF_NETDIAG_CLIENT_ENABLE
#define OPENTHREAD_CONFIG_TMF_NETDIAG_CLIENT_ENABLE   1
#endif
// </e>
// <e>  Time Synchronization Service
#define OPENTHREAD_CONFIG_TIME_SYNC_ENABLE          0
// </e>
// <e>  UDP Forward
#ifndef OPENTHREAD_CONFIG_UDP_FORWARD_ENABLE
#define OPENTHREAD_CONFIG_UDP_FORWARD_ENABLE        0
#endif
// </e>
// <e>  Enable Mac beacon payload parsing support
#ifndef OPENTHREAD_CONFIG_MAC_BEACON_PAYLOAD_PARSING_ENABLE
#define OPENTHREAD_CONFIG_MAC_BEACON_PAYLOAD_PARSING_ENABLE      1
#endif
// </e>
// <e>  Max raw power calibration length.
#ifndef SL_OPENTHREAD_RAW_POWER_CALIBRATION_LENGTH
#define SL_OPENTHREAD_RAW_POWER_CALIBRATION_LENGTH  4
#endif
// </e>
// <e>  Max FEM config setting length.
#ifndef SL_OPENTHREAD_FEM_SETTING_LENGTH
#define SL_OPENTHREAD_FEM_SETTING_LENGTH            4
#endif
// </e>
// <i> The maximum number of RX buffers to use in the radio driver.
// <d> 16
#ifndef SL_OPENTHREAD_RADIO_RX_BUFFER_COUNT
#define SL_OPENTHREAD_RADIO_RX_BUFFER_COUNT       16
#endif
// </h>
// <h>  Logging
// <o   OPENTHREAD_CONFIG_LOG_OUTPUT> LOG_OUTPUT
//      <OPENTHREAD_CONFIG_LOG_OUTPUT_NONE             => NONE
//      <OPENTHREAD_CONFIG_LOG_OUTPUT_APP              => APP
//      <OPENTHREAD_CONFIG_LOG_OUTPUT_PLATFORM_DEFINED => PLATFORM_DEFINED
// <i>  Default: OPENTHREAD_CONFIG_LOG_OUTPUT_PLATFORM_DEFINED
#ifndef OPENTHREAD_CONFIG_LOG_OUTPUT
#define OPENTHREAD_CONFIG_LOG_OUTPUT OPENTHREAD_CONFIG_LOG_OUTPUT_APP
#endif

// <q>  DYNAMIC_LOG_LEVEL
#ifndef OPENTHREAD_CONFIG_LOG_LEVEL_DYNAMIC_ENABLE
#define OPENTHREAD_CONFIG_LOG_LEVEL_DYNAMIC_ENABLE  0
#endif

// <e>  Enable Logging
#define OPENTHREAD_FULL_LOGS_ENABLE                 0
#if     OPENTHREAD_FULL_LOGS_ENABLE

// <h>  Note: Enabling higher log levels, which include logging packet details, can cause delays which may result in join failures.
// <o   OPENTHREAD_CONFIG_LOG_LEVEL> LOG_LEVEL
//      <OT_LOG_LEVEL_NONE       => NONE
//      <OT_LOG_LEVEL_CRIT       => CRIT
//      <OT_LOG_LEVEL_WARN       => WARN
//      <OT_LOG_LEVEL_NOTE       => NOTE
//      <OT_LOG_LEVEL_INFO       => INFO
//      <OT_LOG_LEVEL_DEBG       => DEBG
// <i>  Default: OT_LOG_LEVEL_DEBG
#ifndef OPENTHREAD_CONFIG_LOG_LEVEL
#define OPENTHREAD_CONFIG_LOG_LEVEL OT_LOG_LEVEL_DEBG
#endif
// <q>  CLI
#ifndef OPENTHREAD_CONFIG_LOG_CLI
#define OPENTHREAD_CONFIG_LOG_CLI                   1
#endif
// <q>  PKT_DUMP
#ifndef OPENTHREAD_CONFIG_LOG_PKT_DUMP
#define OPENTHREAD_CONFIG_LOG_PKT_DUMP              1
#endif
// <q>  PLATFORM
#ifndef OPENTHREAD_CONFIG_LOG_PLATFORM
#define OPENTHREAD_CONFIG_LOG_PLATFORM              1
#endif
// <q>  PREPEND_LEVEL
#ifndef OPENTHREAD_CONFIG_LOG_PREPEND_LEVEL
#define OPENTHREAD_CONFIG_LOG_PREPEND_LEVEL         1
#endif

#endif // OPENTHREAD_FULL_LOGS_ENABLE

// <e> Log crash dump after initialization
#ifndef OPENTHREAD_CONFIG_PLATFORM_LOG_CRASH_DUMP_ENABLE
#define OPENTHREAD_CONFIG_PLATFORM_LOG_CRASH_DUMP_ENABLE 0
#endif
// </e>

// </h>
// </e>
// </h>

// <<< end of configuration section >>>
#endif // _SL_OPENTHREAD_FEATURES_CONFIG_H