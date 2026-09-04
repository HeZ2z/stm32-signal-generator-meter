#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "board/board_config.h"
#include "signal_gen/signal_gen_dac.h"
#include "signal_gen/signal_gen_dac_logic.h"

static void test_validate_square_config(void) {
  assert(signal_gen_dac_config_valid(&(signal_gen_dac_config_t){
      .frequency_hz = 1000U, .waveform = APP_DAC_WAVE_SQUARE}));
  assert(signal_gen_dac_config_valid(&(signal_gen_dac_config_t){
      .frequency_hz = 1000U, .waveform = APP_DAC_WAVE_TRIANGLE}));
  assert(!signal_gen_dac_config_valid(&(signal_gen_dac_config_t){
      .frequency_hz = 10U, .waveform = APP_DAC_WAVE_SQUARE}));
  assert(signal_gen_dac_config_valid(&(signal_gen_dac_config_t){
      .frequency_hz = 1000U, .waveform = APP_DAC_WAVE_SINE}));
}

static void test_compute_tim6_update_rate(void) {
  uint32_t update_hz = 0U;

  assert(signal_gen_dac_compute_tim6_update_hz(APP_DAC_WAVE_SQUARE, 1000U, &update_hz));
  assert(update_hz == 256000U);
  assert(signal_gen_dac_compute_tim6_update_hz(APP_DAC_WAVE_TRIANGLE, 1000U, &update_hz));
  assert(update_hz == 256000U);
  assert(!signal_gen_dac_compute_tim6_update_hz(APP_DAC_WAVE_SQUARE, 0U, &update_hz));
  assert(signal_gen_dac_compute_tim6_update_hz(APP_DAC_WAVE_SINE, 1000U, &update_hz));
  assert(update_hz == 256000U);
}

static void test_compute_tim6_dividers(void) {
  signal_gen_dac_tim6_config_t config = {0};

  assert(signal_gen_dac_compute_tim6_config(84000000U, APP_DAC_WAVE_SQUARE, 20U,
                                            &config));
  assert(config.period <= 0xFFFFU);
  assert(config.achieved_update_hz > 0U);

  assert(signal_gen_dac_compute_tim6_config(84000000U, APP_DAC_WAVE_TRIANGLE, 1000U,
                                            &config));
  assert(config.prescaler == 0U);
  assert(config.period == 327U);
  assert(config.achieved_update_hz == 256097U);

  assert(signal_gen_dac_compute_tim6_config(84000000U, APP_DAC_WAVE_SQUARE, 100000U,
                                            &config));
  assert(config.period <= 0xFFFFU);
  assert(config.achieved_update_hz == 28000000U);
}

static void test_pack_dual_samples(void) {
  assert(signal_gen_dac_pack_dual_12b(0x0123U, 0x0456U) == 0x04560123UL);
  assert(signal_gen_dac_pack_dual_12b(0x1ABCU, 0x2DEFU) == 0x0DEF0ABCUL);
  assert(signal_gen_dac_pack_dual_12b(0x1000U, 0x2000U) == 0x00000000UL);
}

static void test_validate_boundaries(void) {
  // NULL
  assert(!signal_gen_dac_config_valid(NULL));

  // 频率下界
  assert(!signal_gen_dac_config_valid(&(signal_gen_dac_config_t){
      .frequency_hz = APP_DAC_MIN_FREQ_HZ - 1U, .waveform = APP_DAC_WAVE_SINE}));
  assert(signal_gen_dac_config_valid(&(signal_gen_dac_config_t){
      .frequency_hz = APP_DAC_MIN_FREQ_HZ, .waveform = APP_DAC_WAVE_SINE}));
  assert(signal_gen_dac_config_valid(&(signal_gen_dac_config_t){
      .frequency_hz = APP_DAC_MIN_FREQ_HZ, .waveform = APP_DAC_WAVE_SQUARE}));
  assert(signal_gen_dac_config_valid(&(signal_gen_dac_config_t){
      .frequency_hz = APP_DAC_MIN_FREQ_HZ, .waveform = APP_DAC_WAVE_TRIANGLE}));

  // 频率上界
  assert(signal_gen_dac_config_valid(&(signal_gen_dac_config_t){
      .frequency_hz = APP_DAC_MAX_FREQ_HZ, .waveform = APP_DAC_WAVE_SINE}));
  assert(!signal_gen_dac_config_valid(&(signal_gen_dac_config_t){
      .frequency_hz = APP_DAC_MAX_FREQ_HZ + 1U, .waveform = APP_DAC_WAVE_SINE}));

  // 无效 waveform
  assert(!signal_gen_dac_config_valid(&(signal_gen_dac_config_t){
      .frequency_hz = 1000U, .waveform = 99}));
  assert(!signal_gen_dac_config_valid(&(signal_gen_dac_config_t){
      .frequency_hz = 1000U, .waveform = (dac_waveform_t)(-1)}));
}

int main(void) {
  printf("Running signal_gen_dac_logic tests...\n");
  test_validate_square_config();
  test_compute_tim6_update_rate();
  test_compute_tim6_dividers();
  test_pack_dual_samples();
  test_validate_boundaries();
  printf("All signal_gen_dac_logic tests passed.\n");
  return 0;
}
