#ifndef SIGNAL_CAPTURE_ADC_H
#define SIGNAL_CAPTURE_ADC_H

#include <stdbool.h>
#include <stdint.h>

#include "board/board_config.h"

typedef struct {
  bool valid;
  bool flat_signal;
  uint16_t samples[APP_SCOPE_SAMPLE_COUNT];
  uint16_t sample_count;
  uint16_t min_raw;
  uint16_t max_raw;
  uint16_t mean_raw;
  uint32_t latest_update_ms;
} scope_capture_snapshot_t;

typedef struct {
  scope_capture_snapshot_t ch_a;
  scope_capture_snapshot_t ch_b;
} scope_capture_frame_t;

void signal_capture_adc_init(void);
void signal_capture_adc_dma_irq_handler(void);
void signal_capture_adc_read_frame(scope_capture_frame_t *out, uint32_t now_ms);
uint32_t signal_capture_adc_channel_sample_rate_hz(void);

#endif
