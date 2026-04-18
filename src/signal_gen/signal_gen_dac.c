#include "signal_gen/signal_gen_dac.h"

#include <string.h>

#include "main.h"

void signal_gen_dac_init(void) {
  /* TODO(M9): 初始化 DAC1(PA4) 和 DAC2(PA5) GPIO 复用。
   * TODO(M9): 配置 DAC 输出缓冲区与触发源。
   * TODO(M9): 配置 DMA 通道指向 DAC 缓冲区。
   * TODO(M9): 初始化方波波形表（可由 signal_gen_dac_set_waveform 切换）。 */
}

bool signal_gen_dac_set_waveform(uint8_t channel,
                                  dac_waveform_t type,
                                  uint32_t frequency_hz) {
  (void)channel;
  (void)type;
  (void)frequency_hz;

  /* TODO(M9): 根据 channel/type/frequency_hz 配置波形表指针和 DMA 传输节奏。
   * - 方波：固定占空比方波表，通过调整 DMA 传输频率改变输出频率。
   * - 正弦波：256 点正弦查表。
   * - 三角波：线性递增/递减查表。
   * - channel 0 → DAC1(PA4)，channel 1 → DAC2(PA5)。 */
  return false;
}

void signal_gen_dac_poll(uint32_t now_ms) {
  (void)now_ms;
  /* TODO(M9): 在此处理波形切换、频率平滑更新等动态逻辑。 */
}
