#ifndef SCOPE_RENDER_LOGIC_H
#define SCOPE_RENDER_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#include "board/board_config.h"

typedef struct {
  bool valid;
  bool flat_signal;
  uint16_t point_count;
  uint16_t min_raw;
  uint16_t max_raw;
  uint16_t mean_raw;
  uint16_t y_points[APP_SCOPE_SAMPLE_COUNT];
} scope_render_trace_t;

typedef struct {
  bool valid;
  uint32_t frequency_hz;
  uint8_t duty_percent;
} scope_square_wave_estimate_t;

bool scope_render_build_trace(const uint16_t *samples,
                              uint16_t sample_count,
                              uint16_t plot_height,
                              uint16_t flat_threshold,
                              scope_render_trace_t *trace);

bool scope_square_wave_frequency_window(uint16_t sample_count,
                                        uint32_t sample_rate_hz,
                                        uint32_t *min_frequency_hz,
                                        uint32_t *max_frequency_hz);

bool scope_square_wave_signal_present(const uint16_t *samples,
                                      uint16_t sample_count,
                                      uint16_t flat_threshold,
                                      uint16_t min_span,
                                      uint16_t low_level_max,
                                      uint16_t high_level_min);

bool scope_estimate_square_wave(const uint16_t *samples,
                                uint16_t sample_count,
                                uint16_t flat_threshold,
                                uint32_t sample_rate_hz,
                                scope_square_wave_estimate_t *estimate);

#endif
