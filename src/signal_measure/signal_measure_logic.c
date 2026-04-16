#include "signal_measure/signal_measure_logic.h"

#include <stddef.h>

/* 将输入捕获得到的 tick 值换算为频率、周期和占空比。 */
bool signal_measure_compute_result(uint32_t period_ticks,
                                   uint32_t high_ticks,
                                   uint32_t timer_tick_hz,
                                   signal_measure_result_t *result) {
  uint32_t duty_scaled;

  if (result == NULL || timer_tick_hz == 0U || period_ticks == 0U || high_ticks > period_ticks) {
    return false;
  }

  result->valid = true;
  result->frequency_hz = timer_tick_hz / period_ticks;
  result->period_us = (period_ticks * 1000000U) / timer_tick_hz;

  /* 通过四舍五入减少整除截断带来的占空比抖动。 */
  duty_scaled = ((high_ticks * 100U) + (period_ticks / 2U)) / period_ticks;
  if (duty_scaled > 100U) {
    duty_scaled = 100U;
  }
  result->duty_percent = (uint8_t)duty_scaled;

  return result->frequency_hz != 0U && result->period_us != 0U;
}
