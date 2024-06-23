/***************************************************************************//**
 * @file
 * @brief OpenThread Green Power configuration file.
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

//-------- <<< Use Configuration Wizard in Context Menu >>> -----------------

// <e>  Rx Offset time
// The rx offset time for the bidirectional message.
//
#define GP_RX_OFFSET_IN_MICRO_SECONDS  21000
// </e>

// <e>  TX Timeout
// Tx timeout after which the gp outgoing packet is invalid.
//
#define GP_TX_MAX_TIMEOUT_IN_MICRO_SECONDS  5000000
// </e>
// <<< end of configuration section >>>
