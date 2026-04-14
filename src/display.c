#include "display.h"

#include <stdbool.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "main.h"

typedef struct {
  UART_HandleTypeDef handle;
  USART_TypeDef *instance;
  GPIO_TypeDef *port;
  uint16_t tx_pin;
  uint16_t rx_pin;
  uint8_t af;
  const char *label;
} uart_console_t;

static uart_console_t consoles[] = {
    {.instance = USART1, .port = GPIOA, .tx_pin = GPIO_PIN_9, .rx_pin = GPIO_PIN_10, .af = GPIO_AF7_USART1, .label = "USART1 PA9/PA10"},
    {.instance = USART2, .port = GPIOA, .tx_pin = GPIO_PIN_2, .rx_pin = GPIO_PIN_3, .af = GPIO_AF7_USART2, .label = "USART2 PA2/PA3"},
    {.instance = USART3, .port = GPIOB, .tx_pin = GPIO_PIN_10, .rx_pin = GPIO_PIN_11, .af = GPIO_AF7_USART3, .label = "USART3 PB10/PB11"},
};

static UART_HandleTypeDef *primary_uart = &consoles[0].handle;

static void enable_uart_clock(USART_TypeDef *instance) {
  if (instance == USART1) {
    __HAL_RCC_USART1_CLK_ENABLE();
  } else if (instance == USART2) {
    __HAL_RCC_USART2_CLK_ENABLE();
  } else if (instance == USART3) {
    __HAL_RCC_USART3_CLK_ENABLE();
  }
}

static void uart_gpio_init(const uart_console_t *console) {
  GPIO_InitTypeDef gpio_init = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  gpio_init.Pin = console->tx_pin | console->rx_pin;
  gpio_init.Mode = GPIO_MODE_AF_PP;
  gpio_init.Pull = GPIO_PULLUP;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init.Alternate = console->af;
  HAL_GPIO_Init(console->port, &gpio_init);
}

void display_init(void) {
  for (size_t i = 0; i < (sizeof(consoles) / sizeof(consoles[0])); ++i) {
    enable_uart_clock(consoles[i].instance);
    uart_gpio_init(&consoles[i]);

    consoles[i].handle.Instance = consoles[i].instance;
    consoles[i].handle.Init.BaudRate = APP_UART_BAUDRATE;
    consoles[i].handle.Init.WordLength = UART_WORDLENGTH_8B;
    consoles[i].handle.Init.StopBits = UART_STOPBITS_1;
    consoles[i].handle.Init.Parity = UART_PARITY_NONE;
    consoles[i].handle.Init.Mode = UART_MODE_TX_RX;
    consoles[i].handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    consoles[i].handle.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&consoles[i].handle) != HAL_OK) {
      Error_Handler();
    }
  }
}

void display_write(const char *text) {
  HAL_UART_Transmit(primary_uart, (uint8_t *)text, (uint16_t)strlen(text), HAL_MAX_DELAY);
}

void display_printf(const char *fmt, ...) {
  char buffer[192];
  va_list args;

  va_start(args, fmt);
  int written = vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  if (written > 0) {
    size_t length = (size_t)written;
    if (length >= sizeof(buffer)) {
      length = sizeof(buffer) - 1U;
    }
    HAL_UART_Transmit(primary_uart, (uint8_t *)buffer, (uint16_t)length, HAL_MAX_DELAY);
  }
}

void display_boot_banner(void) {
  display_printf("\r\n[%s]\r\n", BOARD_NAME);
  display_printf("Clock=%lu Hz, UART=%lu baud\r\n", HAL_RCC_GetSysClockFreq(), APP_UART_BAUDRATE);
  display_printf("Console=%s\r\n", consoles[0].label);
  display_printf("PWM=PB6(TIM4_CH1)\r\n");
  display_printf("MEAS=PB5(TIM3_CH2), loopback PB6->PB5\r\n");
}

void display_help(void) {
  display_write("Commands: help | status | freq <hz> | duty <1-99>\r\n");
  display_write("Example: freq 2000 ; duty 30 ; status\r\n");
  display_write("Loopback: PB6(TIM4_CH1) -> PB5(TIM3_CH2)\r\n");
}

void display_status(const signal_gen_config_t *config, const signal_measure_result_t *measurement) {
  int32_t freq_error;
  int32_t period_error;
  int32_t duty_error;

  if (measurement != NULL && measurement->valid) {
    freq_error = (int32_t)measurement->frequency_hz - (int32_t)config->frequency_hz;
    period_error = (int32_t)measurement->period_us -
                   (int32_t)(1000000UL / config->frequency_hz);
    duty_error = (int32_t)measurement->duty_percent - (int32_t)config->duty_percent;

    display_printf("SET freq=%luHz duty=%u%% | MEAS freq=%luHz period=%luus duty=%u%% | ERR df=%" PRId32 "Hz dt=%" PRId32 "us dd=%" PRId32 "%%\r\n",
                   config->frequency_hz,
                   config->duty_percent,
                   measurement->frequency_hz,
                   measurement->period_us,
                   measurement->duty_percent,
                   freq_error,
                   period_error,
                   duty_error);
    return;
  }

  display_printf("SET freq=%luHz duty=%u%% | MEAS no-signal | check PB6->PB5 loopback\r\n",
                 config->frequency_hz,
                 config->duty_percent);
}

UART_HandleTypeDef *display_uart_handle(void) {
  return primary_uart;
}
