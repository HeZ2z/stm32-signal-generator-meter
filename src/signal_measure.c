#include "signal_measure.h"

#include <string.h>

#include "main.h"
#include "signal_measure_logic.h"

static TIM_HandleTypeDef measure_timer;
static volatile signal_measure_result_t latest_result;
static volatile uint32_t last_capture_ms;

static uint32_t timer_clock_hz(void) {
  uint32_t pclk = HAL_RCC_GetPCLK1Freq();
  uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE1);

  if (ppre == RCC_HCLK_DIV1) {
    return pclk;
  }

  return pclk * 2U;
}

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

void signal_measure_poll(uint32_t now_ms) {
  if (latest_result.valid &&
      (now_ms - last_capture_ms) >= APP_MEASURE_SIGNAL_TIMEOUT_MS) {
    latest_result.valid = false;
  }
}

void signal_measure_irq_handler(void) {
  HAL_TIM_IRQHandler(&measure_timer);
}

const signal_measure_result_t *signal_measure_latest(void) {
  return (const signal_measure_result_t *)&latest_result;
}

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

  if (!signal_measure_compute_result(period_ticks, high_ticks, APP_TIMER_TICK_HZ, &next_result)) {
    latest_result.valid = false;
    return;
  }

  latest_result = next_result;
  last_capture_ms = HAL_GetTick();
}
