/**
 * @file
 * Serial API Configuration
 * @copyright 2022 Silicon Laboratories Inc.
 */
#ifndef SERIAL_API_CONFIG_H
#define SERIAL_API_CONFIG_H

#include <em_gpio.h>

// <<< sl:start pin_tool >>>

// <usart signal=TX,RX> SERIAL_API

// $[USART_SERIAL_API]
#ifndef SERIAL_API_PERIPHERAL                   
#define SERIAL_API_PERIPHERAL                    USART0
#endif
#ifndef SERIAL_API_PERIPHERAL_NO                
#define SERIAL_API_PERIPHERAL_NO                 0
#endif

// USART0 TX on PA08
#ifndef SERIAL_API_TX_PORT                      
#define SERIAL_API_TX_PORT                       gpioPortA
#endif
#ifndef SERIAL_API_TX_PIN                       
#define SERIAL_API_TX_PIN                        8
#endif

// USART0 RX on PA09
#ifndef SERIAL_API_RX_PORT                      
#define SERIAL_API_RX_PORT                       gpioPortA
#endif
#ifndef SERIAL_API_RX_PIN                       
#define SERIAL_API_RX_PIN                        9
#endif
// [USART_SERIAL_API]$

// <<< sl:end pin_tool >>>

#endif // SERIAL_API_CONFIG_H
