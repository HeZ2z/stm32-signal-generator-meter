#include "signal_gen_logic.h"

#include <stddef.h>

#include "board_config.h"

bool signal_gen_config_valid(const signal_gen_config_t *config) {
  if (config == NULL) {
    return false;
  }

  if (config->frequency_hz < APP_PWM_MIN_FREQ_HZ ||
      config->frequency_hz > APP_PWM_MAX_FREQ_HZ ||
      config->duty_percent < APP_PWM_MIN_DUTY_PERCENT ||
      config->duty_percent > APP_PWM_MAX_DUTY_PERCENT) {
    return false;
  }

  return true;
}

bool signal_gen_compute_params(const signal_gen_config_t *config,
                               uint32_t timer_clock_hz,
                               uint32_t *prescaler,
                               uint32_t *period,
                               uint32_t *pulse) {
  uint32_t target_ticks;

  if (!signal_gen_config_valid(config) || prescaler == NULL || period == NULL || pulse == NULL) {
    return false;
  }

  if (config->frequency_hz == 0U || timer_clock_hz < APP_TIMER_TICK_HZ) {
    return false;
  }

  target_ticks = APP_TIMER_TICK_HZ / config->frequency_hz;
  if (target_ticks < 2U || target_ticks > 65535U) {
    return false;
  }

  *prescaler = (timer_clock_hz / APP_TIMER_TICK_HZ) - 1U;
  *period = target_ticks - 1U;
  *pulse = (target_ticks * config->duty_percent) / 100U;
  return true;
}
