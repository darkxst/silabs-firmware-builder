/*******************************************************************************
 * @file
 * @brief Core application logic.
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

// Define module name for Power Manager debuging feature.
#define CURRENT_MODULE_NAME "OPENTHREAD_SAMPLE_APP"

#include <assert.h>
#include <openthread-core-config.h>
#include <openthread/config.h>

#include <openthread/cli.h>
#include <openthread/diag.h>
#include <openthread/tasklet.h>

#include "app.h"
#include "openthread-system.h"

#include "reset_util.h"

#include "sl_component_catalog.h"
#include "sl_memory_manager.h"
#ifdef SL_CATALOG_POWER_MANAGER_PRESENT
#include "sl_power_manager.h"
#endif

#ifdef SL_CATALOG_KERNEL_PRESENT
#include "sl_ot_rtos_adaptation.h"
#endif // SL_CATALOG_KERNEL_PRESENT

#if (defined(SL_CATALOG_BTN0_PRESENT) || defined(SL_CATALOG_BTN1_PRESENT))
#include "sl_button.h"
#include "sl_simple_button.h"
#endif

/**
 * This function initializes the CLI app.
 *
 * @param[in]  aInstance  The OpenThread instance structure.
 *
 */
extern void otAppCliInit(otInstance *aInstance);

#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
static uint8_t *sOtInstanceBuffer = NULL;
#endif
static otInstance *sInstance      = NULL;
static bool        sButtonPressed = false;
static bool        sStayAwake     = true;

otInstance *otGetInstance(void)
{
    return sInstance;
}

#if (defined(SL_CATALOG_BTN0_PRESENT) || defined(SL_CATALOG_BTN1_PRESENT))
void sl_button_on_change(const sl_button_t *handle)
{
    if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED)
    {
        sButtonPressed = true;
#ifdef SL_CATALOG_KERNEL_PRESENT
        sl_ot_rtos_set_pending_event(SL_OT_RTOS_EVENT_APP);
#endif
        otSysEventSignalPending();
    }
}
#endif

void sl_ot_rtos_application_tick(void)
{
    if (sButtonPressed)
    {
        sButtonPressed = false;
        sStayAwake     = !sStayAwake;
        if (sStayAwake)
        {
#ifdef SL_CATALOG_POWER_MANAGER_PRESENT
            sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
#endif
        }
        else
        {
#ifdef SL_CATALOG_POWER_MANAGER_PRESENT
            sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);
#endif
        }
    }
}

/*
 * Provide, if required an "otPlatLog()" function
 */
#if OPENTHREAD_CONFIG_LOG_OUTPUT == OPENTHREAD_CONFIG_LOG_OUTPUT_APP
void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    OT_UNUSED_VARIABLE(aLogLevel);
    OT_UNUSED_VARIABLE(aLogRegion);
    OT_UNUSED_VARIABLE(aFormat);

    va_list ap;
    va_start(ap, aFormat);
    otCliPlatLogv(aLogLevel, aLogRegion, aFormat, ap);
    va_end(ap);
}
#endif

void sl_ot_create_instance(void)
{
#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
    size_t otInstanceBufferLength = 0;

    // Call to query the buffer size
    (void)otInstanceInit(NULL, &otInstanceBufferLength);

    // Call to allocate the buffer
    sOtInstanceBuffer = (uint8_t *)sl_malloc(otInstanceBufferLength);
    assert(sOtInstanceBuffer);

    // Initialize OpenThread with the buffer
    sInstance = otInstanceInit(sOtInstanceBuffer, &otInstanceBufferLength);
#else
    sInstance = otInstanceInitSingle();
#endif
    assert(sInstance);
}

void sl_ot_cli_init(void)
{
    otAppCliInit(sInstance);
}

/******************************************************************************
 * Application Init.
 *****************************************************************************/

void app_init(void)
{
    OT_SETUP_RESET_JUMP(argv);
}

/******************************************************************************
 * Application Process Action.
 *****************************************************************************/
void app_process_action(void)
{
    otTaskletsProcess(sInstance);
    otSysProcessDrivers(sInstance);
}

/******************************************************************************
 * Application Exit.
 *****************************************************************************/
void app_exit(void)
{
    otInstanceFinalize(sInstance);
#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
    sl_free(sOtInstanceBuffer);
#endif
    // TO DO : pseudo reset?
}
