#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "signal_measure/signal_measure_logic.h"

static int failures = 0;

static void expect(bool condition, const char *message) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s\n", message);
    failures++;
  }
}

static void test_compute_2000hz_30pct(void) {
  signal_measure_result_t result = {0};

  expect(signal_measure_compute_result(500U, 150U, 1000000U, &result), "2000Hz/30% compute success");
  expect(result.valid, "result valid");
  expect(result.frequency_hz == 2000U, "frequency 2000Hz");
  expect(result.period_us == 500U, "period 500us");
  expect(result.duty_percent == 30U, "duty 30 percent");
}

static void test_compute_other_ratios(void) {
  signal_measure_result_t result = {0};

  expect(signal_measure_compute_result(1000U, 500U, 1000000U, &result), "1000Hz/50% compute success");
  expect(result.frequency_hz == 1000U, "frequency 1000Hz");
  expect(result.period_us == 1000U, "period 1000us");
  expect(result.duty_percent == 50U, "duty 50 percent");

  expect(signal_measure_compute_result(200U, 140U, 1000000U, &result), "5000Hz/70% compute success");
  expect(result.frequency_hz == 5000U, "frequency 5000Hz");
  expect(result.period_us == 200U, "period 200us");
  expect(result.duty_percent == 70U, "duty 70 percent");
}

static void test_compute_rejects_bad_inputs(void) {
  signal_measure_result_t result = {0};

  expect(!signal_measure_compute_result(0U, 0U, 1000000U, &result), "zero period rejected");
  expect(!signal_measure_compute_result(100U, 101U, 1000000U, &result), "high greater than period rejected");
  expect(!signal_measure_compute_result(100U, 50U, 0U, &result), "zero timer tick rejected");
}

static void test_wraparound_delta_example(void) {
  signal_measure_result_t result = {0};
  uint32_t first = 65500U;
  uint32_t second = 164U;
  uint32_t delta = ((0xFFFFU - first) + second) + 1U;

  expect(delta == 200U, "wraparound delta example");
  expect(signal_measure_compute_result(delta, 100U, 1000000U, &result), "wraparound derived result valid");
  expect(result.frequency_hz == 5000U, "wraparound frequency 5000Hz");
  expect(result.duty_percent == 50U, "wraparound duty 50 percent");
}

int main(void) {
  test_compute_2000hz_30pct();
  test_compute_other_ratios();
  test_compute_rejects_bad_inputs();
  test_wraparound_delta_example();

  if (failures != 0) {
    return 1;
  }

  puts("PASS: test_signal_measure_logic");
  return 0;
}
