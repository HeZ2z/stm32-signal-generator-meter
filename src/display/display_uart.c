#include "display/display_backend.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "signal_capture/signal_capture_adc.h"
#include "signal_gen/signal_gen_dac.h"
#include "touch/touch.h"

/* 支持同时初始化多组串口，当前默认选择 consoles[0] 作为主控制台。 */
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

/* 根据串口实例打开对应 RCC 时钟。 */
static void enable_uart_clock(USART_TypeDef *instance) {
  if (instance == USART1) {
    __HAL_RCC_USART1_CLK_ENABLE();
  } else if (instance == USART2) {
    __HAL_RCC_USART2_CLK_ENABLE();
  } else if (instance == USART3) {
    __HAL_RCC_USART3_CLK_ENABLE();
  }
}

/* 初始化串口 GPIO 复用。 */
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

/* 初始化所有预留控制台，默认使用第一组作为当前活动串口。 */
void display_uart_init(void) {
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

    /* 任一控制台初始化失败都视为致命错误，因为后续调试将失去文本输出。 */
    if (HAL_UART_Init(&consoles[i].handle) != HAL_OK) {
      Error_Handler();
    }
  }
}

/* 串口输出采用阻塞发送，确保日志顺序与主流程一致。 */
void display_uart_write(const char *text) {
  HAL_UART_Transmit(primary_uart, (uint8_t *)text, (uint16_t)strlen(text), HAL_MAX_DELAY);
}

/* 输出板卡、时钟和回环接线等最关键的启动信息。 */
void display_uart_boot_banner(void) {
  char buffer[192];
  const touch_runtime_t *touch = touch_runtime();

  int written = snprintf(buffer,
                         sizeof(buffer),
                         "\r\n[%s]\r\nClock=%lu Hz, UART=%lu baud\r\nConsole=%s\r\nDAC=PA4/PA5 via TIM6+DMA\r\nADC=PA0/PA6 via TIM2 trigger\r\nTouch=GT9XXX PH6/PI3/PI8/PH7\r\nTouchInit=%s%s%s\r\n",
                         BOARD_NAME,
                         HAL_RCC_GetSysClockFreq(),
                         (unsigned long)APP_UART_BAUDRATE,
                         consoles[0].label,
                         touch->status,
                         touch->controller[0] != '\0' ? " ID=" : "",
                         touch->controller[0] != '\0' ? touch->controller : "");
  if (written > 0) {
    display_uart_write(buffer);
  }
}

/* 帮助信息保持纯文本，方便普通串口工具直接查看。 */
void display_uart_help(void) {
  display_uart_write("Commands: help | status | freq <hz> | duty <1-99>\r\n");
  display_uart_write("Touch: tap F-1K F+1K WAVE YT/XY RESET MORE on LCD\r\n");
  display_uart_write("M10: square / triangle, duty is not user-adjustable\r\n");
  display_uart_write("MORE: show project info on LCD, help stays on UART\r\n");
  display_uart_write("Loopback: PA4(DAC1)->PA0(ADC1) and PA5(DAC2)->PA6(ADC1)\r\n");
}

/* 将当前设定值、实测值和 LCD 状态拼成一行串口状态文本。 */
void display_uart_status(void) {
  char buffer[256];
  const char *lcd_state = display_lcd_status_impl();
  const touch_runtime_t *touch = touch_runtime();
  const signal_gen_dac_status_t *dac = signal_gen_dac_current();
  scope_capture_frame_t frame = {0};
  const char *wave = "SQUARE";
  uint32_t now = HAL_GetTick();

  signal_capture_adc_read_frame(&frame, now);

  if (dac->waveform == APP_DAC_WAVE_TRIANGLE) {
    wave = "TRIANGLE";
  }

  int written = snprintf(
      buffer, sizeof(buffer),
      "DAC %s freq=%luHz | ADC CH-A=%s CH-B=%s @%luHz | LCD %s | TOUCH %s%s%s\r\n",
      wave,
      (unsigned long)dac->frequency_hz,
      frame.ch_a.valid ? "LIVE" : "NO-SIGNAL",
      frame.ch_b.valid ? "LIVE" : "NO-SIGNAL",
      (unsigned long)signal_capture_adc_channel_sample_rate_hz(),
      lcd_state,
      touch->status,
      touch->controller[0] != '\0' ? " ID=" : "",
      touch->controller[0] != '\0' ? touch->controller : "");
  if (written > 0) {
    display_uart_write(buffer);
  }
}

/* UI 控制层通过这个接口获取当前主串口。 */
UART_HandleTypeDef *display_uart_handle(void) {
  return primary_uart;
}
