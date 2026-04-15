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

/* 初始化板载双色 LED，用于心跳和错误提示。 */
static void led_init(void) {
  GPIO_InitTypeDef gpio_init = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  gpio_init.Pin = GPIO_PIN_0 | GPIO_PIN_1;
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &gpio_init);
}

/* 配置系统主时钟到 168 MHz，并额外提供 LTDC 像素时钟。 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef osc_init = {0};
  RCC_ClkInitTypeDef clk_init = {0};
  RCC_PeriphCLKInitTypeDef periph_clk_init = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  osc_init.HSIState = RCC_HSI_ON;
  osc_init.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  osc_init.PLL.PLLState = RCC_PLL_ON;
  osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  osc_init.PLL.PLLM = 16;
  osc_init.PLL.PLLN = 336;
  osc_init.PLL.PLLP = RCC_PLLP_DIV2;
  osc_init.PLL.PLLQ = 7;

  if (HAL_RCC_OscConfig(&osc_init) != HAL_OK) {
    Error_Handler();
  }

  clk_init.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk_init.APB1CLKDivider = RCC_HCLK_DIV4;
  clk_init.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&clk_init, FLASH_LATENCY_5) != HAL_OK) {
    Error_Handler();
  }

  periph_clk_init.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
  periph_clk_init.PLLSAI.PLLSAIN = 180;
  periph_clk_init.PLLSAI.PLLSAIR = 5;
  periph_clk_init.PLLSAIDivR = RCC_PLLSAIDIVR_4;

  if (HAL_RCCEx_PeriphCLKConfig(&periph_clk_init) != HAL_OK) {
    Error_Handler();
  }

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000U);
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
}

/* 出现不可恢复错误时，通过 LED 闪烁次数输出错误码。 */
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

/* 按“显示 -> 发生 -> 测量 -> 控制”的顺序拉起所有业务模块。 */
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
  display_write("init: starting lcd\r\n");
  display_lcd_start();
  display_printf("init: lcd %s\r\n", display_lcd_state());

  display_boot_banner();
  display_write("Stable demo image: UART + PWM + command control\r\n");
  display_write("LED heartbeat active on PB0/PB1\r\n");
  display_help();

  /* 默认参数必须在启动阶段先落地，后续 UI 命令都基于这份当前配置修改。 */
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

/* 单次轮询负责心跳、命令处理、测量超时退化和周期性状态输出。 */
void app_run_once(void) {
  uint32_t now = HAL_GetTick();
  const signal_gen_config_t *config = signal_gen_current();
  const signal_measure_result_t *measurement = signal_measure_latest();

  /* 固件以固定节奏翻转 LED，便于快速确认主循环仍在运行。 */
  if ((now - last_blink_ms) >= 250U) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1);
    last_blink_ms = now;
  }

  ui_ctrl_poll();
  signal_measure_poll(now);

  /* 状态输出频率故意低于主循环速度，避免串口和 LCD 被无意义刷屏。 */
  if ((now - last_status_ms) >= APP_STATUS_PERIOD_MS) {
    display_status(config, measurement);
    last_status_ms = now;
  }
}
