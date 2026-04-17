#include "scope/scope_render_logic.h"

#include <stddef.h>

static bool scope_signal_bounds(const uint16_t *samples,
                                uint16_t sample_count,
                                uint16_t *min_raw,
                                uint16_t *max_raw,
                                uint32_t *sum) {
  uint16_t local_min = 0xFFFFU;
  uint16_t local_max = 0U;
  uint32_t local_sum = 0U;

  if (samples == NULL || min_raw == NULL || max_raw == NULL || sum == NULL ||
      sample_count == 0U || sample_count > APP_SCOPE_SAMPLE_COUNT) {
    return false;
  }

  for (uint16_t i = 0; i < sample_count; ++i) {
    if (samples[i] < local_min) {
      local_min = samples[i];
    }
    if (samples[i] > local_max) {
      local_max = samples[i];
    }
    local_sum += samples[i];
  }

  *min_raw = local_min;
  *max_raw = local_max;
  *sum = local_sum;
  return true;
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
  uint16_t first_rising = 0U;
  uint16_t second_rising = 0U;
  uint16_t falling = 0U;
  bool prev_high;
  bool found_first_rising = false;
  bool found_falling = false;
  bool found_second_rising = false;

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

    if (!found_first_rising) {
      if (!prev_high && high) {
        first_rising = i;
        found_first_rising = true;
      }
    } else if (!found_falling) {
      if (prev_high && !high) {
        falling = i;
        found_falling = true;
      }
    } else if (!prev_high && high) {
      second_rising = i;
      found_second_rising = true;
      break;
    }

    prev_high = high;
  }

  if (!found_first_rising || !found_falling || !found_second_rising ||
      second_rising <= first_rising || falling <= first_rising || falling >= second_rising) {
    return false;
  }

  estimate->frequency_hz = (sample_rate_hz + ((uint32_t)(second_rising - first_rising) / 2U)) /
                           (uint32_t)(second_rising - first_rising);
  estimate->duty_percent = (uint8_t)((((uint32_t)(falling - first_rising) * 100U) +
                                      ((uint32_t)(second_rising - first_rising) / 2U)) /
                                     (uint32_t)(second_rising - first_rising));
  estimate->valid = estimate->frequency_hz != 0U &&
                    estimate->duty_percent != 0U &&
                    estimate->duty_percent < 100U;
  return estimate->valid;
}
