#ifndef DISPLAY_LCD_HW_H
#define DISPLAY_LCD_HW_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

/* 供外部模块访问的 LCD/SDRAM 句柄和就绪标志。 */
extern LTDC_HandleTypeDef lcd_handle;
extern SDRAM_HandleTypeDef sdram_handle;
extern bool lcd_hw_ready;
extern char lcd_hw_state[64];

/* LCD 硬件初始化（SDRAM → Probe → LTDC → TestPattern → Backlight）。 */
void display_lcd_hw_init(void);

/* LCD 状态字符串（由 lcd_set_state() 写入，供 facade 返回）。 */
const char *display_lcd_hw_state(void);

/* LTDC 中断路由（由 facade 中转）。 */
void display_lcd_hw_irq_handler(void);

#endif
