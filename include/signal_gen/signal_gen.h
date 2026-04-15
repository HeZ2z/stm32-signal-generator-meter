#ifndef SIGNAL_GEN_H
#define SIGNAL_GEN_H

#include <stdbool.h>
#include <stdint.h>

/* PWM 输出当前使用的频率和占空比配置。 */
typedef struct {
  uint32_t frequency_hz;
  uint8_t duty_percent;
} signal_gen_config_t;

/* 初始化 PWM 输出所需的定时器和 GPIO。 */
void signal_gen_init(void);
/* 应用一组新的 PWM 参数，失败时保持旧配置不变。 */
bool signal_gen_apply(const signal_gen_config_t *config);
/* 获取当前已经生效的 PWM 配置。 */
const signal_gen_config_t *signal_gen_current(void);

#endif
