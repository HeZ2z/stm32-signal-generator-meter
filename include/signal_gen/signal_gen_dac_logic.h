#ifndef SIGNAL_GEN_DAC_LOGIC_H
#define SIGNAL_GEN_DAC_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#include "signal_gen/signal_gen_dac.h"

bool signal_gen_dac_config_valid(const signal_gen_dac_config_t *config);
bool signal_gen_dac_compute_tim6_update_hz(uint32_t output_frequency_hz,
                                           uint32_t *update_hz);
uint32_t signal_gen_dac_pack_dual_12b(uint16_t ch_a, uint16_t ch_b);

#endif
