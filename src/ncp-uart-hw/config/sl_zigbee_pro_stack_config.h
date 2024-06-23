/***************************************************************************//**
 * @brief ZigBee PRO Full Stack component configuration header.
 *\n*******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
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
// <<< Use Configuration Wizard in Context Menu >>>

// <h>Zigbee PRO Stack Library configuration

// <o SL_ZIGBEE_MAX_END_DEVICE_CHILDREN> Child Table Size <0-64>
// <i> Default: 6
// <i> The maximum number of ZigBee PRO End Devices that can be supported by a single device.
#define SL_ZIGBEE_MAX_END_DEVICE_CHILDREN   32

// <h> Packet Buffer Heap Allocation
// <o SL_ZIGBEE_PACKET_BUFFER_HEAP_SIZE > Packet Buffer Heap Size <512-16384>
// <i> Default: SL_ZIGBEE_MEDIUM_PACKET_BUFFER_HEAP
// <i> The amount of heap space that is allocated for packet buffers (in bytes).  Each packet buffer has an overhead of `4 * sizeof(uint16_t)` bytes.
// <SL_ZIGBEE_TINY_PACKET_BUFFER_HEAP=> Tiny (1024)
// <SL_ZIGBEE_SMALL_PACKET_BUFFER_HEAP=> Small (2048)
// <SL_ZIGBEE_MEDIUM_PACKET_BUFFER_HEAP=> Medium (4096)
// <SL_ZIGBEE_LARGE_PACKET_BUFFER_HEAP=> Large (8192)
// <SL_ZIGBEE_HUGE_PACKET_BUFFER_HEAP=> Huge (16384)
// <SL_ZIGBEE_CUSTOM_PACKET_BUFFER_HEAP=> Custom
// <d> SL_ZIGBEE_LARGE_PACKET_BUFFER_HEAP
// <o SL_ZIGBEE_CUSTOM_PACKET_BUFFER_HEAP> Custom Heap Size <1024-16384>
// <i> Specify the exact number of bytes to use (aligned on 4-byte boundaries)
#define SL_ZIGBEE_TINY_PACKET_BUFFER_HEAP 1024
#define SL_ZIGBEE_SMALL_PACKET_BUFFER_HEAP 2048
#define SL_ZIGBEE_MEDIUM_PACKET_BUFFER_HEAP 4096
#define SL_ZIGBEE_LARGE_PACKET_BUFFER_HEAP 8192
#define SL_ZIGBEE_HUGE_PACKET_BUFFER_HEAP 16384
#define SL_ZIGBEE_CUSTOM_PACKET_BUFFER_HEAP 0
#define SL_ZIGBEE_PACKET_BUFFER_HEAP_SIZE  SL_ZIGBEE_LARGE_PACKET_BUFFER_HEAP
// </h>

// <o SL_ZIGBEE_END_DEVICE_KEEP_ALIVE_SUPPORT_MODE> End Device keep alive support mode
// <i> End Device keep alive support mode
// <SL_802154_DATA_POLL_KEEP_ALIVE=> MAC Data Poll Keep Alive
// <SL_ZIGBEE_END_DEVICE_TIMEOUT_KEEP_ALIVE=> End Device Timeout Keep Alive
// <SL_ZIGBEE_KEEP_ALIVE_SUPPORT_ALL=> Keep Alive Support All
// <i> Default: SL_ZIGBEE_KEEP_ALIVE_SUPPORT_ALL
// <i> End Device keep alive support mode on the coordinator/router could be set here.
#define SL_ZIGBEE_END_DEVICE_KEEP_ALIVE_SUPPORT_MODE   SL_ZIGBEE_KEEP_ALIVE_SUPPORT_ALL

// <o SL_ZIGBEE_END_DEVICE_POLL_TIMEOUT> End Device Poll Timeout Value
// <i> End Device Poll Timeout Value
// <SECONDS_10=> Seconds-10
// <MINUTES_2=> Minutes-2
// <MINUTES_4=> Minutes-4
// <MINUTES_8=> Minutes-8
// <MINUTES_16=> Minutes-16
// <MINUTES_32=> Minutes-32
// <MINUTES_64=> Minutes-64
// <MINUTES_128=> Minutes-128
// <MINUTES_256=> Minutes-256
// <MINUTES_512=> Minutes-512
// <MINUTES_1024=> Minutes-1024
// <MINUTES_2048=> Minutes-2048
// <MINUTES_4096=> Minutes-4096
// <MINUTES_8192=> Minutes-8192
// <MINUTES_16384=> Minutes-16384
// <i> Default: MINUTES_256
// <i> The amount of time that must pass without hearing a MAC data poll from the device before the end device is removed from the child table.  For a router device this applies to its children.  For an end device, this is the amount of time before it automatically times itself out.
#define SL_ZIGBEE_END_DEVICE_POLL_TIMEOUT   MINUTES_256

// <o SL_ZIGBEE_LINK_POWER_DELTA_INTERVAL> Link Power Delta Request Interval <1-65535>
// <i> Default: 300
// <i> The amount of time in seconds that pass between link power delta requests.
#define SL_ZIGBEE_LINK_POWER_DELTA_INTERVAL   300

// <o SL_ZIGBEE_APS_UNICAST_MESSAGE_COUNT> APS Unicast Message Queue Size <1-255>
// <i> Default: 10
// <i> The maximum number of APS unicast messages that can be queued up by the stack.  A message is considered queued when sli_zigbee_stack_send_unicast() is called and is de-queued when the sli_zigbee_stack_message_sent_handler() is called.
#define SL_ZIGBEE_APS_UNICAST_MESSAGE_COUNT   32

// <o SL_ZIGBEE_APS_DUPLICATE_REJECTION_MAX_ENTRIES> APS unicast Message Duplicate Rejection table Size <1-255>
// <i> Default: 5
// <i> The maximum number of APS unicast messages that can be stored in the stack, to reject duplicate processing/forwarding of APS messages.
// <i> Size of 1 is basically the same thing as no duplicate rejection
#define SL_ZIGBEE_APS_DUPLICATE_REJECTION_MAX_ENTRIES 5

// <o SL_ZIGBEE_BROADCAST_TABLE_SIZE> Broadcast Table Size <15-254>
// <i> Default: 15
// <i> The size of the broadcast table.
#define SL_ZIGBEE_BROADCAST_TABLE_SIZE   64

// <o SL_ZIGBEE_NEIGHBOR_TABLE_SIZE> Neighbor Table Size
// <i> Neighbor Table Size
// <16=> 16
// <26=> 26
// <i> Default: 16
// <i> The size of the neighbor table.
#define SL_ZIGBEE_NEIGHBOR_TABLE_SIZE   16

// <o SL_ZIGBEE_TRANSIENT_KEY_TIMEOUT_S> Transient key timeout (in seconds) <0-65535>
// <i> Default: 300
// <i> The amount of time a device will store a transient link key that can be used to join a network.
#define SL_ZIGBEE_TRANSIENT_KEY_TIMEOUT_S   300

// <o SL_ZIGBEE_BINDING_TABLE_SIZE> Binding Table Size <1-127>
// <i> Default: 3
// <i> The number of entries that the binding table can hold.
#define SL_ZIGBEE_BINDING_TABLE_SIZE   32

// </h>

// <<< end of configuration section >>>
