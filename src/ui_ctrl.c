#include "ui_ctrl.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "main.h"
#include "signal_gen.h"
static char command_buffer[48];
static uint8_t command_length;
static UART_HandleTypeDef *console_uart;

static UART_HandleTypeDef *ui_uart(void) {
  return display_uart_handle();
}

static void handle_command(const char *line) {
  signal_gen_config_t next = *signal_gen_current();
  unsigned long value = 0;

  if (strcmp(line, "help") == 0) {
    display_help();
    return;
  }

  if (strcmp(line, "status") == 0) {
    const signal_gen_config_t *config = signal_gen_current();
    display_status(config);
    return;
  }

  if (sscanf(line, "freq %lu", &value) == 1) {
    next.frequency_hz = value;
    if (signal_gen_apply(&next)) {
      display_printf("OK freq=%lu\r\n", next.frequency_hz);
    } else {
      display_printf("ERR freq range=%u..%u\r\n", APP_PWM_MIN_FREQ_HZ, APP_PWM_MAX_FREQ_HZ);
    }
    return;
  }

  if (sscanf(line, "duty %lu", &value) == 1) {
    next.duty_percent = (uint8_t)value;
    if (signal_gen_apply(&next)) {
      display_printf("OK duty=%u\r\n", next.duty_percent);
    } else {
      display_printf("ERR duty range=%u..%u\r\n", APP_PWM_MIN_DUTY_PERCENT, APP_PWM_MAX_DUTY_PERCENT);
    }
    return;
  }

  display_write("ERR unknown command\r\n");
  display_help();
}

void ui_ctrl_init(void) {
  console_uart = ui_uart();
  command_length = 0U;
}

void ui_ctrl_poll(void) {
  uint8_t ch;

  if (HAL_UART_Receive(console_uart, &ch, 1U, 0U) != HAL_OK) {
    return;
  }

  if (ch == '\r' || ch == '\n') {
    if (command_length > 0U) {
      command_buffer[command_length] = '\0';
      handle_command(command_buffer);
      command_length = 0U;
    }
    return;
  }

  if (command_length < (sizeof(command_buffer) - 1U)) {
    command_buffer[command_length++] = (char)ch;
  } else {
    command_length = 0U;
    display_write("ERR command too long\r\n");
  }
}
