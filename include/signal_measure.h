#ifndef SIGNAL_MEASURE_H
#define SIGNAL_MEASURE_H

#include <stdint.h>

#include "signal_measure_logic.h"

void signal_measure_init(void);
void signal_measure_poll(uint32_t now_ms);
void signal_measure_irq_handler(void);
const signal_measure_result_t *signal_measure_latest(void);

#endif
