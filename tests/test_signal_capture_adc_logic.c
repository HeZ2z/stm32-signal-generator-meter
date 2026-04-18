#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "board/board_config.h"
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

int main(void) {
  printf("Running snapshot_bounds tests...\n");
  test_rejects_null_args();
  test_computes_min_max_sum();
  test_flat_signal_all_equal();
  test_matches_scope_render_logic_behavior();
  printf("All snapshot_bounds tests passed.\n");
  return 0;
}
