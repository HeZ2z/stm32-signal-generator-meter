#include "app/app.h"

#include <stdbool.h>

#include "display/display.h"
#include "main.h"
#include "signal_gen/signal_gen.h"
#include "signal_measure/signal_measure.h"
#include "ui/ui_ctrl.h"

static uint32_t last_blink_ms;
static uint32_t last_status_ms;
static volatile uint32_t error_code;

static void led_init(void) {
  GPIO_InitTypeDef gpio_init = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  gpio_init.Pin = GPIO_PIN_0 | GPIO_PIN_1;
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &gpio_init);
}

void SystemClock_Config(void) {
  RCC_OscInitTypeDef osc_init = {0};
  RCC_ClkInitTypeDef clk_init = {0};

  osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  osc_init.HSIState = RCC_HSI_ON;
  osc_init.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  osc_init.PLL.PLLState = RCC_PLL_NONE;

  if (HAL_RCC_OscConfig(&osc_init) != HAL_OK) {
    Error_Handler();
  }

  clk_init.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk_init.APB1CLKDivider = RCC_HCLK_DIV1;
  clk_init.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&clk_init, FLASH_LATENCY_0) != HAL_OK) {
    Error_Handler();
  }

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000U);
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
}

void Error_Handler(void) {
  while (1) {
    for (uint32_t i = 0; i < (error_code == 0U ? 1U : error_code); ++i) {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
      HAL_Delay(120U);
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
      HAL_Delay(180U);
    }
    HAL_Delay(700U);
  }
}

void app_init(void) {
  HAL_Init();
  SystemClock_Config();
  led_init();

  display_init();
  display_write("init: uart ok\r\n");
  signal_gen_init();
  display_write("init: pwm ok\r\n");
  signal_measure_init();
  display_write("init: measure ok\r\n");
  ui_ctrl_init();

  display_boot_banner();
  display_write("Stable demo image: UART + PWM + command control\r\n");
  display_write("LED heartbeat active on PB0/PB1\r\n");
  display_help();

  if (!signal_gen_apply(&(signal_gen_config_t){
          .frequency_hz = APP_DEFAULT_PWM_FREQ_HZ,
          .duty_percent = APP_DEFAULT_PWM_DUTY_PERCENT,
      })) {
    error_code = 2U;
    Error_Handler();
  }

  last_blink_ms = HAL_GetTick();
  last_status_ms = HAL_GetTick();
}

void app_run_once(void) {
  uint32_t now = HAL_GetTick();
  const signal_gen_config_t *config = signal_gen_current();
  const signal_measure_result_t *measurement = signal_measure_latest();

  if ((now - last_blink_ms) >= 250U) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1);
    last_blink_ms = now;
  }

  ui_ctrl_poll();
  signal_measure_poll(now);

  if ((now - last_status_ms) >= APP_STATUS_PERIOD_MS) {
    display_status(config, measurement);
    last_status_ms = now;
  }
}
