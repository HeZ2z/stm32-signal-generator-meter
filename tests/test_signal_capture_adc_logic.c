#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "board/board_config.h"
#include "signal_capture/signal_capture_adc.h"
#include "signal_capture/signal_capture_adc_logic.h"

static void test_rejects_null_args(void) {
  uint16_t min_raw = 0U, max_raw = 0U;
  uint32_t sum = 0U;
  uint16_t samples[4] = {0};

  assert(!snapshot_bounds(NULL, 4, &min_raw, &max_raw, &sum));
  assert(!snapshot_bounds(samples, 0, &min_raw, &max_raw, &sum));
  assert(!snapshot_bounds(samples, 4, NULL, &max_raw, &sum));
  assert(!snapshot_bounds(samples, 4, &min_raw, NULL, &sum));
  assert(!snapshot_bounds(samples, 4, &min_raw, &max_raw, NULL));
}

static void test_computes_min_max_sum(void) {
  uint16_t min_raw = 0U, max_raw = 0U;
  uint32_t sum = 0U;
  uint16_t samples[4] = {500U, 1000U, 2000U, 4095U};

  assert(snapshot_bounds(samples, 4, &min_raw, &max_raw, &sum));
  assert(min_raw == 500U);
  assert(max_raw == 4095U);
  assert(sum == 7595U);
}

static void test_flat_signal_all_equal(void) {
  uint16_t min_raw = 0U, max_raw = 0U;
  uint32_t sum = 0U;
  uint16_t samples[3] = {2048U, 2048U, 2048U};

  assert(snapshot_bounds(samples, 3, &min_raw, &max_raw, &sum));
  assert(min_raw == 2048U);
  assert(max_raw == 2048U);
  assert(sum == 6144U);
}

static void test_matches_scope_render_logic_behavior(void) {
  uint16_t min_raw = 0U, max_raw = 0U;
  uint32_t sum = 0U;
  uint16_t samples[APP_SCOPE_SAMPLE_COUNT];

  for (uint16_t i = 0; i < APP_SCOPE_SAMPLE_COUNT; ++i) {
    samples[i] = (uint16_t)(i * 16U);
  }

  assert(snapshot_bounds(samples, APP_SCOPE_SAMPLE_COUNT, &min_raw, &max_raw, &sum));
  assert(min_raw == 0U);
  assert(max_raw == (uint16_t)((APP_SCOPE_SAMPLE_COUNT - 1U) * 16U));
  assert(sum == (uint32_t)((APP_SCOPE_SAMPLE_COUNT - 1U) * APP_SCOPE_SAMPLE_COUNT / 2U) * 16U);
}

static void test_unpack_ch_a_live_ch_b_flat(void) {
  uint16_t raw[APP_SCOPE_ADC_CHUNK_RAW_COUNT] = {0};
  scope_capture_frame_t frame = {0};

  for (uint16_t i = 0U; i < APP_SCOPE_SAMPLE_COUNT; ++i) {
    raw[i * 2U] = 0U;
    raw[i * 2U + 1U] = (uint16_t)(1000U + i);
  }

  assert(adc_logic_unpack_interleaved_frame(raw, APP_SCOPE_ADC_CHUNK_RAW_COUNT, 123U,
                                            &frame));
  assert(frame.ch_a.sample_count == APP_SCOPE_SAMPLE_COUNT);
  assert(frame.ch_a.samples[0] == 1000U);
  assert(frame.ch_b.samples[0] == 0U);
  assert(!frame.ch_a.flat_signal);
  assert(frame.ch_b.flat_signal);
}

static void test_unpack_ch_b_live_ch_a_flat(void) {
  uint16_t raw[APP_SCOPE_ADC_CHUNK_RAW_COUNT] = {0};
  scope_capture_frame_t frame = {0};

  for (uint16_t i = 0U; i < APP_SCOPE_SAMPLE_COUNT; ++i) {
    raw[i * 2U] = (uint16_t)(2000U + i);
    raw[i * 2U + 1U] = 0U;
  }

  assert(adc_logic_unpack_interleaved_frame(raw, APP_SCOPE_ADC_CHUNK_RAW_COUNT, 456U,
                                            &frame));
  assert(frame.ch_a.flat_signal);
  assert(!frame.ch_b.flat_signal);
  assert(frame.ch_b.samples[APP_SCOPE_SAMPLE_COUNT - 1U] ==
         (uint16_t)(2000U + APP_SCOPE_SAMPLE_COUNT - 1U));
}

static void test_unpack_dual_live(void) {
  uint16_t raw[APP_SCOPE_ADC_CHUNK_RAW_COUNT] = {0};
  scope_capture_frame_t frame = {0};

  for (uint16_t i = 0U; i < APP_SCOPE_SAMPLE_COUNT; ++i) {
    raw[i * 2U] = (uint16_t)((i % 4U) < 2U ? 512U : 3584U);
    raw[i * 2U + 1U] = (uint16_t)((i % 2U) == 0U ? 0U : 4095U);
  }

  assert(adc_logic_unpack_interleaved_frame(raw, APP_SCOPE_ADC_CHUNK_RAW_COUNT, 789U,
                                            &frame));
  assert(frame.ch_a.valid);
  assert(frame.ch_b.valid);
}

static void test_unpack_rejects_invalid_lengths(void) {
  uint16_t raw[8] = {0};
  scope_capture_frame_t frame = {0};

  assert(!adc_logic_unpack_interleaved_frame(NULL, 8U, 0U, &frame));
  assert(!adc_logic_unpack_interleaved_frame(raw, 7U, 0U, &frame));
  assert(!adc_logic_unpack_interleaved_frame(raw, 2U, 0U, NULL));
}

int main(void) {
  printf("Running snapshot_bounds tests...\n");
  test_rejects_null_args();
  test_computes_min_max_sum();
  test_flat_signal_all_equal();
  test_matches_scope_render_logic_behavior();
  test_unpack_ch_a_live_ch_b_flat();
  test_unpack_ch_b_live_ch_a_flat();
  test_unpack_dual_live();
  test_unpack_rejects_invalid_lengths();
  printf("All snapshot_bounds tests passed.\n");
  return 0;
}
