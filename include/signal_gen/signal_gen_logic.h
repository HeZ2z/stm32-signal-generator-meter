#ifndef SIGNAL_GEN_LOGIC_H
#define SIGNAL_GEN_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#include "signal_gen/signal_gen.h"

bool signal_gen_config_valid(const signal_gen_config_t *config);
bool signal_gen_compute_params(const signal_gen_config_t *config,
                               uint32_t timer_clock_hz,
                               uint32_t *prescaler,
                               uint32_t *period,
                               uint32_t *pulse);

#endif
