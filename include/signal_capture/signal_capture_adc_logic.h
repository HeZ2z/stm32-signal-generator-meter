#ifndef SIGNAL_CAPTURE_ADC_LOGIC_H
#define SIGNAL_CAPTURE_ADC_LOGIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board/board_config.h"
#include "signal_capture/signal_capture_adc.h"

/* 纯逻辑函数：对采样数组进行边界扫描，返回 min / max / sum。
 * 用于替换 signal_capture_adc.c 中 update_snapshot_from_offset() 的内联计算，
 * 以及 scope_render_logic.c 中的 scope_signal_bounds()（两者逻辑相同）。 */
static inline bool snapshot_bounds(const uint16_t *samples,
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

  for (uint16_t i = 0U; i < sample_count; ++i) {
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

bool adc_logic_build_snapshot(const uint16_t *samples,
                              uint16_t sample_count,
                              uint32_t now_ms,
                              scope_capture_snapshot_t *snapshot);

bool adc_logic_unpack_interleaved_frame(const uint16_t *interleaved,
                                        uint16_t raw_count,
                                        uint32_t now_ms,
                                        scope_capture_frame_t *frame);

#endif
