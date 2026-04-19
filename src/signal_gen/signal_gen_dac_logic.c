#include "signal_gen/signal_gen_dac_logic.h"

#include <stddef.h>

#include "board/board_config.h"

bool signal_gen_dac_config_valid(const signal_gen_dac_config_t *config) {
  if (config == NULL) {
    return false;
  }

  if (config->waveform != APP_DAC_WAVE_SQUARE) {
    return false;
  }

  return config->frequency_hz >= APP_DAC_MIN_FREQ_HZ &&
         config->frequency_hz <= APP_DAC_MAX_FREQ_HZ;
}

bool signal_gen_dac_compute_tim6_update_hz(uint32_t output_frequency_hz,
                                           uint32_t *update_hz) {
  if (update_hz == NULL || output_frequency_hz < APP_DAC_MIN_FREQ_HZ ||
      output_frequency_hz > APP_DAC_MAX_FREQ_HZ) {
    return false;
  }

  *update_hz = output_frequency_hz * 2U;
  return *update_hz >= output_frequency_hz;
}

uint32_t signal_gen_dac_pack_dual_12b(uint16_t ch_a, uint16_t ch_b) {
  return ((uint32_t)(ch_b & 0x0FFFU) << 16U) | (uint32_t)(ch_a & 0x0FFFU);
}
