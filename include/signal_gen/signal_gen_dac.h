#ifndef SIGNAL_GEN_DAC_H
#define SIGNAL_GEN_DAC_H

#include <stdbool.h>
#include <stdint.h>

/* 双路 DAC 波形类型（供后续阶段选择）。 */
typedef enum {
  APP_DAC_WAVE_SQUARE = 0,
  APP_DAC_WAVE_SINE,
  APP_DAC_WAVE_TRIANGLE,
} dac_waveform_t;

typedef struct {
  uint32_t frequency_hz;
  dac_waveform_t waveform;
} signal_gen_dac_config_t;

typedef struct {
  bool active;
  uint32_t frequency_hz;
  dac_waveform_t waveform;
} signal_gen_dac_status_t;

/* 初始化双路 DAC 外设与 DMA 传输。 */
void signal_gen_dac_init(void);

bool signal_gen_dac_apply(const signal_gen_dac_config_t *config);
const signal_gen_dac_status_t *signal_gen_dac_current(void);
void signal_gen_dac_dma_irq_handler(void);

void signal_gen_dac_poll(uint32_t now_ms);

const char *signal_gen_dac_waveform_short_name(dac_waveform_t waveform);

#endif
