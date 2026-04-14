#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#ifndef HOST_TEST
#include "stm32f4xx_hal.h"
#endif

#define BOARD_NAME "Apollo STM32F4 (temporary F429 mapping)"

#define APP_UART_BAUDRATE 115200U
#define APP_DEFAULT_PWM_FREQ_HZ 1000U
#define APP_DEFAULT_PWM_DUTY_PERCENT 50U
#define APP_STATUS_PERIOD_MS 500U

#define APP_PWM_MIN_FREQ_HZ 20U
#define APP_PWM_MAX_FREQ_HZ 100000U
#define APP_PWM_MIN_DUTY_PERCENT 1U
#define APP_PWM_MAX_DUTY_PERCENT 99U

#define APP_TIMER_TICK_HZ 1000000U

/*
 * These mappings are provisional until the exact Apollo core-board variant is
 * verified. Only this file should need changes when the real routing is known.
 */
#ifndef HOST_TEST
#define APP_UART_INSTANCE USART1
#define APP_UART_GPIO_PORT GPIOA
#define APP_UART_TX_PIN GPIO_PIN_9
#define APP_UART_RX_PIN GPIO_PIN_10
#define APP_UART_GPIO_AF GPIO_AF7_USART1

#define APP_PWM_TIM_INSTANCE TIM4
#define APP_PWM_GPIO_PORT GPIOB
#define APP_PWM_PIN GPIO_PIN_6
#define APP_PWM_GPIO_AF GPIO_AF2_TIM4
#define APP_PWM_CHANNEL TIM_CHANNEL_1
#endif

#endif
