#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "signal_gen.h"
#include "stm32f4xx_hal.h"

void display_init(void);
void display_write(const char *text);
void display_printf(const char *fmt, ...);
void display_boot_banner(void);
void display_help(void);
void display_status(const signal_gen_config_t *config);
UART_HandleTypeDef *display_uart_handle(void);

#endif
