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

/* 初始化双路 DAC 外设与 DMA 传输。
 * 必须在 system clock 配置完成后调用。 */
void signal_gen_dac_init(void);

/* 设置指定通道的波形参数（占位：M9 阶段填充方波输出）。 */
bool signal_gen_dac_set_waveform(uint8_t channel,
                                  dac_waveform_t type,
                                  uint32_t frequency_hz);

/* 主循环轮询（当前为空操作，后续填充动态更新逻辑）。 */
void signal_gen_dac_poll(uint32_t now_ms);

#endif
