/**
 * Provides support for BRD8029A (Buttons on EXP Board)
 *
 * @copyright 2022 Silicon Laboratories Inc.
 */

#ifndef RADIO_BOARD_BRD2603A_BUTTON_H
#define RADIO_BOARD_BRD2603A_BUTTON_H

/*************************************************************************/
/* Configure push buttons                                                */
/*************************************************************************/

// <<< Use Configuration Wizard in Context Menu >>>
// <h>Button Configuration
#define PB1_LABEL                   "BTN0"

// <o PB1_ON_VALUE> Button1 ON value
// <1=> Active high
// <0=> Active low
// <d> 0
#define PB1_ON_VALUE       0

// <o PB1_INT_NO> Button1 interrupt pin number
// <d> 3
#define PB1_INT_NO       3

// <q PB1_CAN_WAKEUP_EM4> Button1 wake up from EM4
// <d> 1
#define PB1_CAN_WAKEUP_EM4       1

#define PB2_LABEL                   "BTN1"

// <o PB2_ON_VALUE> Button2 ON value
// <1=> Active high
// <0=> Active low
// <d> 0
#define PB2_ON_VALUE       0

// <o PB2_INT_NO> Button2 interrupt pin number
// <d> 2
#define PB2_INT_NO       2

// <q PB2_CAN_WAKEUP_EM4> Button2 wake up from EM4
// <d> 0
#define PB2_CAN_WAKEUP_EM4       0

// </h>

// <<< end of configuration section >>>

// <<< sl:start pin_tool >>>

// <gpio> PB1_GPIO
// $[GPIO_PB1_GPIO]
#ifndef PB1_GPIO_PORT                           
#define PB1_GPIO_PORT                            gpioPortB
#endif
#ifndef PB1_GPIO_PIN                            
#define PB1_GPIO_PIN                             3
#endif
// [GPIO_PB1_GPIO]$

// <gpio> PB2_GPIO
// $[GPIO_PB2_GPIO]
#ifndef PB2_GPIO_PORT                           
#define PB2_GPIO_PORT                            gpioPortB
#endif
#ifndef PB2_GPIO_PIN                            
#define PB2_GPIO_PIN                             2
#endif
// [GPIO_PB2_GPIO]$

// <<< sl:end pin_tool >>>

#endif /* RADIO_BOARD_BRD2603A_BUTTON_H */
