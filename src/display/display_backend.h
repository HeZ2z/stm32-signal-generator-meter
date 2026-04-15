#ifndef DISPLAY_BACKEND_H
#define DISPLAY_BACKEND_H

#include <stdbool.h>

#include "signal_gen/signal_gen.h"
#include "signal_measure/signal_measure.h"
#include "stm32f4xx_hal.h"

void display_uart_init(void);
void display_uart_write(const char *text);
void display_uart_boot_banner(void);
void display_uart_help(void);
void display_uart_status(const signal_gen_config_t *config, const signal_measure_result_t *measurement);
UART_HandleTypeDef *display_uart_handle(void);

void display_lcd_init(void);
bool display_lcd_ready_impl(void);
const char *display_lcd_name_impl(void);
const char *display_lcd_status_impl(void);
void display_lcd_irq_handler(void);
void display_lcd_write(const char *text);
void display_lcd_boot_banner(void);
void display_lcd_help(void);
void display_lcd_status(const signal_gen_config_t *config, const signal_measure_result_t *measurement);

#endif
