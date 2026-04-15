#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "signal_gen/signal_gen.h"
#include "signal_measure/signal_measure.h"
#include "stm32f4xx_hal.h"

/* 初始化显示子系统，当前先拉起 UART 后端。 */
void display_init(void);
/* 启动 LCD 后端初始化流程。 */
void display_lcd_start(void);
/* 查询 LCD 是否已经可用于正常绘制。 */
bool display_lcd_ready(void);
/* 获取当前 LCD 后端名称。 */
const char *display_lcd_name(void);
/* 获取当前 LCD 状态文本。 */
const char *display_lcd_state(void);
/* 向所有已启用的显示后端写入原始文本。 */
void display_write(const char *text);
/* 以 printf 风格向所有显示后端输出格式化文本。 */
void display_printf(const char *fmt, ...);
/* 输出启动横幅和后端状态。 */
void display_boot_banner(void);
/* 输出帮助信息。 */
void display_help(void);
/* 同步输出当前 SET/MEAS/ERR 状态。 */
void display_status(const signal_gen_config_t *config, const signal_measure_result_t *measurement);
/* 仅刷新 LCD 页面，不向 UART 重复写状态。 */
void display_refresh_lcd(const signal_gen_config_t *config, const signal_measure_result_t *measurement);
/* 获取 UI 控制循环使用的主串口句柄。 */
UART_HandleTypeDef *display_uart_handle(void);

#endif
