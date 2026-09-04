#ifndef SIGNAL_MEASURE_H
#define SIGNAL_MEASURE_H

#include <stdint.h>

#include "signal_measure/signal_measure_logic.h"

/* 初始化输入捕获定时器和中断。 */
void signal_measure_init(void);
/* 轮询无信号超时状态。 */
void signal_measure_poll(uint32_t now_ms);
/* 供 TIM3 IRQ 调用的测量中断入口。 */
void signal_measure_irq_handler(void);
/* 获取最近一次测量结果。 */
const signal_measure_result_t *signal_measure_latest(void);

#endif
