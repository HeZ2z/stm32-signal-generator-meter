#ifndef SIGNAL_GEN_H
#define SIGNAL_GEN_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t frequency_hz;
  uint8_t duty_percent;
} signal_gen_config_t;

void signal_gen_init(void);
bool signal_gen_apply(const signal_gen_config_t *config);
const signal_gen_config_t *signal_gen_current(void);

#endif
