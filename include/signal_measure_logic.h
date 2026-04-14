#ifndef SIGNAL_MEASURE_LOGIC_H
#define SIGNAL_MEASURE_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool valid;
  uint32_t frequency_hz;
  uint32_t period_us;
  uint8_t duty_percent;
} signal_measure_result_t;

bool signal_measure_compute_result(uint32_t period_ticks,
                                   uint32_t high_ticks,
                                   uint32_t timer_tick_hz,
                                   signal_measure_result_t *result);

#endif
