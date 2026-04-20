#ifndef DISPLAY_BACKEND_H
#define DISPLAY_BACKEND_H

#include <stdbool.h>

#include "signal_measure/signal_measure.h"
#include "signal_gen/signal_gen_dac.h"
#ifndef HOST_TEST
#include "stm32f4xx_hal.h"
#endif

/* display.c 门面层依赖的后端接口定义。 */

/* UART 后端接口，负责稳定的文本调试输出。 */
void display_uart_init(void);
void display_uart_write(const char *text);
void display_uart_boot_banner(void);
void display_uart_help(void);
void display_uart_status(void);
#ifndef HOST_TEST
UART_HandleTypeDef *display_uart_handle(void);
#endif

/* LCD 后端接口，负责板载 RGBLCD 的图形状态页。 */
void display_lcd_init(void);
bool display_lcd_ready_impl(void);
const char *display_lcd_name_impl(void);
const char *display_lcd_status_impl(void);
void display_lcd_irq_handler(void);
void display_lcd_write(const char *text);
void display_lcd_boot_banner(void);
void display_lcd_help(void);
void display_lcd_status(void);

#endif
