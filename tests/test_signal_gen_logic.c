#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "signal_gen_logic.h"

static int failures = 0;

static void expect(bool condition, const char *message) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s\n", message);
    failures++;
  }
}

static void test_config_valid(void) {
  expect(signal_gen_config_valid(&(signal_gen_config_t){.frequency_hz = 1000U, .duty_percent = 50U}),
         "default config valid");
  expect(!signal_gen_config_valid(&(signal_gen_config_t){.frequency_hz = 10U, .duty_percent = 50U}),
         "low frequency rejected");
  expect(!signal_gen_config_valid(&(signal_gen_config_t){.frequency_hz = 1000U, .duty_percent = 0U}),
         "low duty rejected");
}

static void test_compute_params(void) {
  uint32_t prescaler = 0;
  uint32_t period = 0;
  uint32_t pulse = 0;

  expect(signal_gen_compute_params(&(signal_gen_config_t){.frequency_hz = 1000U, .duty_percent = 50U},
                                   16000000U,
                                   &prescaler,
                                   &period,
                                   &pulse),
         "parameter compute success");
  expect(prescaler == 15U, "prescaler for 16MHz");
  expect(period == 999U, "period for 1kHz");
  expect(pulse == 500U, "pulse for 50 percent");
}

static void test_compute_rejects_bad_inputs(void) {
  uint32_t prescaler = 0;
  uint32_t period = 0;
  uint32_t pulse = 0;

  expect(!signal_gen_compute_params(&(signal_gen_config_t){.frequency_hz = 1000U, .duty_percent = 50U},
                                    800000U,
                                    &prescaler,
                                    &period,
                                    &pulse),
         "timer clock below tick rejected");
  expect(!signal_gen_compute_params(&(signal_gen_config_t){.frequency_hz = 1000000U, .duty_percent = 50U},
                                    16000000U,
                                    &prescaler,
                                    &period,
                                    &pulse),
         "too high frequency rejected");
}

int main(void) {
  test_config_valid();
  test_compute_params();
  test_compute_rejects_bad_inputs();

  if (failures != 0) {
    return 1;
  }

  puts("PASS: test_signal_gen_logic");
  return 0;
}
