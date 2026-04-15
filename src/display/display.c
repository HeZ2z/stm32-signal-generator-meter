#include "display/display.h"

#include <stdarg.h>
#include <stdio.h>

#include "display/display_backend.h"

void display_init(void) {
  display_uart_init();
}

void display_lcd_start(void) {
  display_lcd_init();
}

bool display_lcd_ready(void) {
  return display_lcd_ready_impl();
}

const char *display_lcd_name(void) {
  return display_lcd_name_impl();
}

const char *display_lcd_state(void) {
  return display_lcd_status_impl();
}

void display_write(const char *text) {
  display_uart_write(text);
  display_lcd_write(text);
}

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

void display_boot_banner(void) {
  display_uart_boot_banner();
  display_printf("LCD=%s (%s)\r\n", display_lcd_name(), display_lcd_state());
  if (display_lcd_ready()) {
    display_lcd_boot_banner();
  }
}

void display_help(void) {
  display_uart_help();
  display_lcd_help();
}

void display_status(const signal_gen_config_t *config, const signal_measure_result_t *measurement) {
  display_uart_status(config, measurement);
  display_lcd_status(config, measurement);
}
