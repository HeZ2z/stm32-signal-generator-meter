#include "scope/scope_render_logic.h"
#include "signal_capture/signal_capture_adc_logic.h"

#include <stddef.h>

/* min/max/sum 扫描委托给 signal_capture_adc_logic.h 中共享的 snapshot_bounds()。 */
static bool scope_signal_bounds(const uint16_t *samples,
                                uint16_t sample_count,
                                uint16_t *min_raw,
                                uint16_t *max_raw,
                                uint32_t *sum) {
  return snapshot_bounds(samples, sample_count, min_raw, max_raw, sum);
}

bool scope_render_build_trace(const uint16_t *samples,
                              uint16_t sample_count,
                              uint16_t plot_height,
                              uint16_t flat_threshold,
                              scope_render_trace_t *trace) {
  uint32_t sum = 0U;
  uint16_t min_raw = 0xFFFFU;
  uint16_t max_raw = 0U;
  uint16_t drawable_height;

  if (samples == NULL || trace == NULL || sample_count == 0U ||
      sample_count > APP_SCOPE_SAMPLE_COUNT || plot_height < 2U) {
    return false;
  }

  drawable_height = (uint16_t)(plot_height - 1U);
  if (!scope_signal_bounds(samples, sample_count, &min_raw, &max_raw, &sum)) {
    return false;
  }

  trace->valid = true;
  trace->point_count = sample_count;
  trace->min_raw = min_raw;
  trace->max_raw = max_raw;
  trace->mean_raw = (uint16_t)(sum / sample_count);
  trace->flat_signal = (uint16_t)(max_raw - min_raw) < flat_threshold;

  for (uint16_t i = 0; i < sample_count; ++i) {
    uint32_t scaled = ((uint32_t)samples[i] * drawable_height) / 4095U;
    trace->y_points[i] = (uint16_t)(drawable_height - scaled);
  }

  return true;
}

bool scope_square_wave_frequency_window(uint16_t sample_count,
                                        uint32_t sample_rate_hz,
                                        uint32_t *min_frequency_hz,
                                        uint32_t *max_frequency_hz) {
  const uint16_t low_freq_margin_samples = 16U;
  const uint16_t high_freq_min_period_samples = 14U;

  if (min_frequency_hz == NULL || max_frequency_hz == NULL ||
      sample_count <= low_freq_margin_samples ||
      sample_rate_hz < high_freq_min_period_samples) {
    return false;
  }

  *min_frequency_hz = (sample_rate_hz + (uint32_t)(sample_count - low_freq_margin_samples) - 1U) /
                      (uint32_t)(sample_count - low_freq_margin_samples);
  *max_frequency_hz = sample_rate_hz / (uint32_t)high_freq_min_period_samples;
  return *min_frequency_hz <= *max_frequency_hz;
}

bool scope_square_wave_signal_present(const uint16_t *samples,
                                      uint16_t sample_count,
                                      uint16_t flat_threshold,
                                      uint16_t min_span,
                                      uint16_t low_level_max,
                                      uint16_t high_level_min) {
  uint16_t min_raw = 0U;
  uint16_t max_raw = 0U;
  uint32_t sum = 0U;
  uint16_t span = 0U;

  if (!scope_signal_bounds(samples, sample_count, &min_raw, &max_raw, &sum)) {
    return false;
  }

  span = (uint16_t)(max_raw - min_raw);
  if (span < flat_threshold || span < min_span) {
    return false;
  }

  return min_raw <= low_level_max && max_raw >= high_level_min;
}

bool scope_estimate_square_wave(const uint16_t *samples,
                                uint16_t sample_count,
                                uint16_t flat_threshold,
                                uint32_t sample_rate_hz,
                                scope_square_wave_estimate_t *estimate) {
  uint16_t min_raw = 0U;
  uint16_t max_raw = 0U;
  uint32_t sum = 0U;
  uint16_t threshold;
  uint16_t rising_edges[8] = {0};
  uint16_t falling_edges[8] = {0};
  uint8_t rising_count = 0U;
  uint8_t falling_count = 0U;
  uint32_t period_sum = 0U;
  uint32_t high_sum = 0U;
  uint16_t first_period = 0U;
  uint8_t complete_cycles = 0U;
  bool prev_high;

  if (estimate == NULL) {
    return false;
  }

  estimate->valid = false;
  estimate->frequency_hz = 0U;
  estimate->duty_percent = 0U;

  if (!scope_square_wave_signal_present(samples,
                                        sample_count,
                                        flat_threshold,
                                        APP_SCOPE_SIGNAL_MIN_SPAN,
                                        APP_SCOPE_SQUARE_LOW_LEVEL_MAX,
                                        APP_SCOPE_SQUARE_HIGH_LEVEL_MIN) ||
      !scope_signal_bounds(samples, sample_count, &min_raw, &max_raw, &sum) ||
      sample_count < 8U || sample_rate_hz == 0U) {
    return false;
  }

  threshold = (uint16_t)((min_raw + max_raw) / 2U);
  prev_high = samples[0] >= threshold;

  for (uint16_t i = 1U; i < sample_count; ++i) {
    bool high = samples[i] >= threshold;

    if (!prev_high && high) {
      if (rising_count < (sizeof(rising_edges) / sizeof(rising_edges[0]))) {
        rising_edges[rising_count++] = i;
      }
    } else if (prev_high && !high) {
      if (falling_count < (sizeof(falling_edges) / sizeof(falling_edges[0]))) {
        falling_edges[falling_count++] = i;
      }
    }

    prev_high = high;
  }

  if (rising_count < 2U) {
    return false;
  }

  for (uint8_t i = 0U; i < (uint8_t)(rising_count - 1U); ++i) {
    uint16_t rise = rising_edges[i];
    uint16_t next_rise = rising_edges[i + 1U];
    uint16_t fall = 0U;

    for (uint8_t j = 0U; j < falling_count; ++j) {
      if (falling_edges[j] > rise && falling_edges[j] < next_rise) {
        fall = falling_edges[j];
        break;
      }
    }

    if (fall == 0U || next_rise <= rise || fall <= rise || fall >= next_rise) {
      continue;
    }

    period_sum += (uint32_t)(next_rise - rise);
    high_sum += (uint32_t)(fall - rise);
    if (first_period == 0U) {
      first_period = (uint16_t)(next_rise - rise);
    }
    ++complete_cycles;
  }

  if (complete_cycles == 0U || period_sum == 0U || first_period == 0U) {
    return false;
  }

  estimate->frequency_hz =
      (sample_rate_hz * (uint32_t)complete_cycles + (period_sum / 2U)) / period_sum;
  estimate->duty_percent = (uint8_t)(((high_sum * 100U) + (period_sum / 2U)) / period_sum);
  estimate->valid = estimate->frequency_hz != 0U &&
                    estimate->duty_percent != 0U &&
                    estimate->duty_percent < 100U;
  return estimate->valid;
}
