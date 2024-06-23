/**
 * Provides support for BRD8029A (LEDs on EXP Board)
 *
 * @copyright 2022 Silicon Laboratories Inc.
 */

#ifndef RADIO_BOARD_BRD2603A_LED_H
#define RADIO_BOARD_BRD2603A_LED_H

/*************************************************************************/
/* Configure LEDs                                                        */
/*************************************************************************/

// <<< Use Configuration Wizard in Context Menu >>>
// <h>LED Configuration
// <o LED1_ON_VALUE> LED1 ON value
// <0=> Active low
// <1=> Active high
// <d> 1
#define LED1_ON_VALUE        1

// <o LED2_ON_VALUE> LED2 ON value
// <0=> Active low
// <1=> Active high
// <d> 1
#define LED2_ON_VALUE        1

// </h>

// <<< end of configuration section >>>

// <<< sl:start pin_tool >>>

#define LED1_LABEL           "LED0"
// <gpio> LED1_GPIO
// $[GPIO_LED1_GPIO]
#ifndef LED1_GPIO_PORT                          
#define LED1_GPIO_PORT                           gpioPortC
#endif
#ifndef LED1_GPIO_PIN                           
#define LED1_GPIO_PIN                            8
#endif
// [GPIO_LED1_GPIO]$

#define LED2_LABEL           "LED1"
// <gpio> LED2_GPIO
// $[GPIO_LED2_GPIO]
#ifndef LED2_GPIO_PORT                          
#define LED2_GPIO_PORT                           gpioPortC
#endif
#ifndef LED2_GPIO_PIN                           
#define LED2_GPIO_PIN                            9
#endif
// [GPIO_LED2_GPIO]$

// <<< sl:end pin_tool >>>

#endif /* RADIO_BOARD_BRD2603A_LED_H */
