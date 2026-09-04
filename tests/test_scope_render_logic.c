#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "board/board_config.h"
#include "scope/scope_render_logic.h"

static void test_rejects_invalid_args(void) {
  scope_render_trace_t trace = {0};
  uint16_t samples[4] = {0};

  assert(!scope_render_build_trace(NULL, 4, 100, 8, &trace));
  assert(!scope_render_build_trace(samples, 0, 100, 8, &trace));
  assert(!scope_render_build_trace(samples, 4, 1, 8, &trace));
  assert(!scope_render_build_trace(samples, 4, 100, 8, NULL));
}

static void test_maps_waveform_to_trace(void) {
  scope_render_trace_t trace = {0};
  uint16_t samples[4] = {0U, 1024U, 2048U, 4095U};

  assert(scope_render_build_trace(samples, 4, 100, 8, &trace));
  assert(trace.valid);
  assert(!trace.flat_signal);
  assert(trace.point_count == 4U);
  assert(trace.min_raw == 0U);
  assert(trace.max_raw == 4095U);
  assert(trace.mean_raw == 1791U);
  assert(trace.y_points[0] == 99U);
  assert(trace.y_points[3] == 0U);
  assert(trace.y_points[1] > trace.y_points[2]);
}

static void test_detects_flat_signal(void) {
  scope_render_trace_t trace = {0};
  uint16_t samples[4] = {2000U, 2004U, 2001U, 2003U};

  assert(scope_render_build_trace(samples, 4, 120, 8, &trace));
  assert(trace.flat_signal);
  assert(trace.min_raw == 2000U);
  assert(trace.max_raw == 2004U);
}

static void fill_square_wave(uint16_t *samples,
                             uint16_t sample_count,
                             uint16_t period_samples,
                             uint16_t high_samples,
                             uint16_t phase_offset) {
  for (uint16_t i = 0; i < sample_count; ++i) {
    uint16_t phase = (uint16_t)((i + phase_offset) % period_samples);
    samples[i] = phase < high_samples ? 4095U : 0U;
  }
}

static void test_frequency_window_matches_scope_budget(void) {
  uint32_t min_frequency_hz = 0U;
  uint32_t max_frequency_hz = 0U;

  assert(scope_square_wave_frequency_window(APP_SCOPE_SAMPLE_COUNT,
                                            218750U,
                                            &min_frequency_hz,
                                            &max_frequency_hz));
  assert(min_frequency_hz == 720U);
  assert(max_frequency_hz == 15625U);
}

static void test_estimates_square_wave_frequency_and_duty(void) {
  scope_square_wave_estimate_t estimate = {0};
  uint16_t samples[APP_SCOPE_SAMPLE_COUNT] = {0};

  fill_square_wave(samples, APP_SCOPE_SAMPLE_COUNT, 40U, 10U, 20U);
  assert(scope_estimate_square_wave(samples,
                                    APP_SCOPE_SAMPLE_COUNT,
                                    APP_SCOPE_SIGNAL_FLAT_THRESHOLD,
                                    200000U,
                                    &estimate));
  assert(estimate.valid);
  assert(estimate.frequency_hz == 5000U);
  assert(estimate.duty_percent == 25U);
}

static void test_rejects_non_rail_noise_as_square_wave(void) {
  scope_square_wave_estimate_t estimate = {0};
  uint16_t samples[APP_SCOPE_SAMPLE_COUNT] = {0};

  for (uint16_t i = 0; i < APP_SCOPE_SAMPLE_COUNT; ++i) {
    samples[i] = (uint16_t)(1900U + ((i % 9U) * 18U));
  }

  assert(!scope_square_wave_signal_present(samples,
                                           APP_SCOPE_SAMPLE_COUNT,
                                           APP_SCOPE_SIGNAL_FLAT_THRESHOLD,
                                           APP_SCOPE_SIGNAL_MIN_SPAN,
                                           APP_SCOPE_SQUARE_LOW_LEVEL_MAX,
                                           APP_SCOPE_SQUARE_HIGH_LEVEL_MIN));
  assert(!scope_estimate_square_wave(samples,
                                     APP_SCOPE_SAMPLE_COUNT,
                                     APP_SCOPE_SIGNAL_FLAT_THRESHOLD,
                                     200000U,
                                     &estimate));
}

static void test_rejects_incomplete_square_wave_cycle(void) {
  scope_square_wave_estimate_t estimate = {0};
  uint16_t samples[APP_SCOPE_SAMPLE_COUNT] = {0};

  fill_square_wave(samples, APP_SCOPE_SAMPLE_COUNT, 400U, 200U, 0U);
  assert(!scope_estimate_square_wave(samples,
                                     APP_SCOPE_SAMPLE_COUNT,
                                     APP_SCOPE_SIGNAL_FLAT_THRESHOLD,
                                     200000U,
                                     &estimate));
  assert(!estimate.valid);
}

int main(void) {
  test_rejects_invalid_args();
  test_maps_waveform_to_trace();
  test_detects_flat_signal();
  test_frequency_window_matches_scope_budget();
  test_estimates_square_wave_frequency_and_duty();
  test_rejects_non_rail_noise_as_square_wave();
  test_rejects_incomplete_square_wave_cycle();
  return 0;
}
