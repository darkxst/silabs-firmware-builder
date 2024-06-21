/***************************************************************************//**
 * @file nc_efr32_wdog.c
 * @brief Legacy HAL Watchdog
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories, Inc, www.silabs.com</b>
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

#include "em_wdog.h"
#include "nc_efr32_wdog.h"

static bool feed_watchdog_on_warn_interrupt;


#if (_SILICON_LABS_32B_SERIES >= 1)
void WDOG0_IRQHandler(void)
{
  uint32_t interrupts;

  interrupts = WDOGn_IntGet(WDOG0);
  WDOGn_IntClear(WDOG0, interrupts);

  if (feed_watchdog_on_warn_interrupt) {
    WDOGn_Feed(WDOG0);
    feed_watchdog_on_warn_interrupt = false;
  }
}
#endif


void nc_enable_watchdog(void)
{
  // Enable LE interface
#if !defined(_SILICON_LABS_32B_SERIES_2)
  CMU_ClockEnable(cmuClock_HFLE, true);
  CMU_OscillatorEnable(cmuOsc_LFRCO, true, true);
#endif

#if defined(_SILICON_LABS_32B_SERIES_2) && !defined(_SILICON_LABS_32B_SERIES_2_CONFIG_1)
  CMU_ClockEnable(cmuClock_WDOG0, true);
#endif

  // Make sure FULL reset is used on WDOG timeout
#if defined(_RMU_CTRL_WDOGRMODE_MASK)
  RMU_ResetControl(rmuResetWdog, rmuResetModeFull);
#endif

  WDOG_Init_TypeDef init = WDOG_INIT_DEFAULT;
  init.enable = false;
  init.debugRun = false;
  init.perSel = wdogPeriod_64k;  // 2 seconds
  init.warnSel = wdogWarnTime75pct;

#if defined(_WDOG_CTRL_CLKSEL_MASK)
  init.clkSel = wdogClkSelLFRCO;
#else
  // Series 2 devices select watchdog oscillator with the CMU.
  CMU_ClockSelectSet(cmuClock_WDOG0, cmuSelect_LFRCO);
#endif

  WDOGn_Unlock(WDOG0);
  WDOGn_Init(WDOG0, &init);

  // Enable warning interrupt
  NVIC_ClearPendingIRQ(WDOG0_IRQn);
  WDOGn_IntClear(WDOG0, WDOG_IF_WARN);
  NVIC_EnableIRQ(WDOG0_IRQn);
  WDOGn_IntEnable(WDOG0, WDOG_IEN_WARN);

  WDOGn_Enable(WDOG0, true);
  WDOGn_Feed(WDOG0);

  feed_watchdog_on_warn_interrupt = false;
}


void nc_poke_watchdog(void)
{
  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();
  feed_watchdog_on_warn_interrupt = true;
  CORE_EXIT_ATOMIC();
}
