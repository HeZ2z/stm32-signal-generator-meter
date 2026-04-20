#include "signal_gen/signal_gen_dac_logic.h"

#include <limits.h>
#include <stddef.h>

#include "board/board_config.h"

bool signal_gen_dac_config_valid(const signal_gen_dac_config_t *config) {
  if (config == NULL) {
    return false;
  }

  if (config->waveform != APP_DAC_WAVE_SQUARE &&
      config->waveform != APP_DAC_WAVE_TRIANGLE) {
    return false;
  }

  return config->frequency_hz >= APP_DAC_MIN_FREQ_HZ &&
         config->frequency_hz <= APP_DAC_MAX_FREQ_HZ;
}

bool signal_gen_dac_compute_tim6_update_hz(dac_waveform_t waveform,
                                           uint32_t output_frequency_hz,
                                           uint32_t *update_hz) {
  uint32_t samples_per_cycle = APP_DAC_WAVE_TABLE_SIZE;

  if (update_hz == NULL || output_frequency_hz < APP_DAC_MIN_FREQ_HZ ||
      output_frequency_hz > APP_DAC_MAX_FREQ_HZ) {
    return false;
  }

  if (waveform != APP_DAC_WAVE_SQUARE && waveform != APP_DAC_WAVE_TRIANGLE) {
    return false;
  }

  if (output_frequency_hz > (UINT32_MAX / samples_per_cycle)) {
    return false;
  }

  *update_hz = output_frequency_hz * samples_per_cycle;
  return true;
}

bool signal_gen_dac_compute_tim6_config(uint32_t timer_clock_hz,
                                        dac_waveform_t waveform,
                                        uint32_t output_frequency_hz,
                                        signal_gen_dac_tim6_config_t *config) {
  uint32_t target_update_hz = 0U;
  uint32_t best_error = UINT_MAX;
  bool found = false;

  if (config == NULL ||
      !signal_gen_dac_compute_tim6_update_hz(waveform, output_frequency_hz,
                                             &target_update_hz) ||
      timer_clock_hz == 0U) {
    return false;
  }

  for (uint32_t prescaler = 0U; prescaler <= 0xFFFFU; ++prescaler) {
    uint64_t divisor = (uint64_t)(prescaler + 1U) * (uint64_t)target_update_hz;
    uint64_t ticks_rounded;
    uint64_t actual_divisor;
    uint32_t achieved_update_hz;
    uint32_t error;

    if (divisor == 0U) {
      continue;
    }

    ticks_rounded = ((uint64_t)timer_clock_hz + (divisor / 2U)) / divisor;
    if (ticks_rounded < 2U || ticks_rounded > 65536U) {
      continue;
    }

    actual_divisor = (uint64_t)(prescaler + 1U) * ticks_rounded;
    achieved_update_hz = (uint32_t)((uint64_t)timer_clock_hz / actual_divisor);
    error = achieved_update_hz > target_update_hz
                ? (achieved_update_hz - target_update_hz)
                : (target_update_hz - achieved_update_hz);

    if (!found || error < best_error ||
        (error == best_error && prescaler < config->prescaler)) {
      config->prescaler = (uint16_t)prescaler;
      config->period = (uint16_t)(ticks_rounded - 1U);
      config->achieved_update_hz = achieved_update_hz;
      best_error = error;
      found = true;

      if (error == 0U && prescaler == 0U) {
        break;
      }
    }
  }

  return found;
}

uint32_t signal_gen_dac_pack_dual_12b(uint16_t ch_a, uint16_t ch_b) {
  return ((uint32_t)(ch_b & 0x0FFFU) << 16U) | (uint32_t)(ch_a & 0x0FFFU);
}
