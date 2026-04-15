#include "display/display.h"

#include <stdarg.h>
#include <stdio.h>

#include "display/display_backend.h"

/* 先初始化最稳定的 UART 后端，保证后续失败时仍有调试输出。 */
void display_init(void) {
  display_uart_init();
}

/* LCD 单独延后启动，避免拉高系统初始化耦合度。 */
void display_lcd_start(void) {
  display_lcd_init();
}

/* 以下查询接口由门面转发到底层 LCD 后端。 */
bool display_lcd_ready(void) {
  return display_lcd_ready_impl();
}

const char *display_lcd_name(void) {
  return display_lcd_name_impl();
}

const char *display_lcd_state(void) {
  return display_lcd_status_impl();
}

/* 原始文本同时送往 UART 与 LCD；LCD 侧当前会忽略不适合直接显示的日志。 */
void display_write(const char *text) {
  display_uart_write(text);
  display_lcd_write(text);
}

/* 统一处理格式化输出，避免多个后端重复写 vsnprintf 逻辑。 */
void display_printf(const char *fmt, ...) {
  char buffer[192];
  va_list args;

  va_start(args, fmt);
  int written = vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  if (written <= 0) {
    return;
  }

  display_write(buffer);
}

/* 启动横幅优先输出到 UART，LCD ready 后再绘制图形化欢迎页。 */
void display_boot_banner(void) {
  display_uart_boot_banner();
  display_printf("LCD=%s (%s)\r\n", display_lcd_name(), display_lcd_state());
  if (display_lcd_ready()) {
    display_lcd_boot_banner();
  }
}

/* 帮助信息和状态信息统一由门面分发到各显示后端。 */
void display_help(void) {
  display_uart_help();
  display_lcd_help();
}

void display_status(const signal_gen_config_t *config, const signal_measure_result_t *measurement) {
  display_uart_status(config, measurement);
  display_lcd_status(config, measurement);
}

void display_refresh_lcd(const signal_gen_config_t *config, const signal_measure_result_t *measurement) {
  display_lcd_status(config, measurement);
}
