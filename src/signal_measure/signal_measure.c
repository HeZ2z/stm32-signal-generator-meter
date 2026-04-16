#include "signal_measure/signal_measure.h"

#include <string.h>

#include "main.h"
#include "signal_measure/signal_measure_logic.h"

static TIM_HandleTypeDef measure_timer;
static volatile signal_measure_result_t latest_result;
static volatile uint32_t last_capture_ms;

/* TIM3 同样挂在 APB1，上层逻辑统一使用真实定时器时钟。 */
static uint32_t timer_clock_hz(void) {
  uint32_t pclk = HAL_RCC_GetPCLK1Freq();
  uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE1);

  if (ppre == RCC_HCLK_DIV1) {
    return pclk;
  }

  return pclk * 2U;
}

/* 配置 TIM3 为 PWM Input 模式，同时抓取周期和高电平宽度。 */
void signal_measure_init(void) {
  TIM_IC_InitTypeDef ic_config = {0};
  TIM_SlaveConfigTypeDef slave_config = {0};
  uint32_t timer_clock = timer_clock_hz();

  measure_timer.Instance = APP_MEASURE_TIM_INSTANCE;
  measure_timer.Init.Prescaler = (timer_clock / APP_TIMER_TICK_HZ) - 1U;
  measure_timer.Init.Period = 0xFFFFU;
  measure_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  measure_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
  measure_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  if (HAL_TIM_IC_Init(&measure_timer) != HAL_OK) {
    Error_Handler();
  }

  /* CH1 走间接输入，抓取下降沿对应的高电平宽度。 */
  ic_config.ICPrescaler = TIM_ICPSC_DIV1;
  ic_config.ICFilter = 0U;

  ic_config.ICPolarity = TIM_ICPOLARITY_FALLING;
  ic_config.ICSelection = TIM_ICSELECTION_INDIRECTTI;
  if (HAL_TIM_IC_ConfigChannel(&measure_timer, &ic_config, APP_MEASURE_CHANNEL_HIGH) != HAL_OK) {
    Error_Handler();
  }

  ic_config.ICPolarity = TIM_ICPOLARITY_RISING;
  ic_config.ICSelection = TIM_ICSELECTION_DIRECTTI;
  if (HAL_TIM_IC_ConfigChannel(&measure_timer, &ic_config, APP_MEASURE_CHANNEL_PERIOD) != HAL_OK) {
    Error_Handler();
  }

  /* 以 TI2 上升沿为复位触发源，保证每个周期都从零开始计数。 */
  slave_config.InputTrigger = TIM_TS_TI2FP2;
  slave_config.SlaveMode = TIM_SLAVEMODE_RESET;
  if (HAL_TIM_SlaveConfigSynchronization(&measure_timer, &slave_config) != HAL_OK) {
    Error_Handler();
  }

  if (HAL_TIM_IC_Start_IT(&measure_timer, APP_MEASURE_CHANNEL_PERIOD) != HAL_OK) {
    Error_Handler();
  }

  if (HAL_TIM_IC_Start_IT(&measure_timer, APP_MEASURE_CHANNEL_HIGH) != HAL_OK) {
    Error_Handler();
  }

  memset((void *)&latest_result, 0, sizeof(latest_result));
  last_capture_ms = 0U;
}

/* 如果长时间没有新捕获，则将结果退化为 no-signal。 */
void signal_measure_poll(uint32_t now_ms) {
  if (latest_result.valid &&
      (now_ms - last_capture_ms) >= APP_MEASURE_SIGNAL_TIMEOUT_MS) {
    latest_result.valid = false;
  }
}

/* 将 TIM3 中断统一转交给 HAL，再由捕获回调做结果换算。 */
void signal_measure_irq_handler(void) {
  HAL_TIM_IRQHandler(&measure_timer);
}

/* 获取当前缓存的最近一次测量结果。 */
const signal_measure_result_t *signal_measure_latest(void) {
  return (const signal_measure_result_t *)&latest_result;
}

/* 仅在周期通道完成一次有效捕获后更新整组测量结果。 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
  signal_measure_result_t next_result = {0};
  uint32_t period_ticks;
  uint32_t high_ticks;

  if (htim->Instance != APP_MEASURE_TIM_INSTANCE ||
      htim->Channel != HAL_TIM_ACTIVE_CHANNEL_2) {
    return;
  }

  period_ticks = HAL_TIM_ReadCapturedValue(htim, APP_MEASURE_CHANNEL_PERIOD);
  high_ticks = HAL_TIM_ReadCapturedValue(htim, APP_MEASURE_CHANNEL_HIGH);

  /* 非法捕获值直接视为本次测量失效，等待下一次有效周期。 */
  if (!signal_measure_compute_result(period_ticks, high_ticks, APP_TIMER_TICK_HZ, &next_result)) {
    latest_result.valid = false;
    return;
  }

  latest_result = next_result;
  last_capture_ms = HAL_GetTick();
}
