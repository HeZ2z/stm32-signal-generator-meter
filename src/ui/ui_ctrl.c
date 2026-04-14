#include "ui/ui_ctrl.h"

#include <stdbool.h>
#include <stdint.h>

#include "display/display.h"
#include "main.h"
#include "signal_gen/signal_gen.h"
#include "signal_measure/signal_measure.h"
#include "ui/ui_cmd.h"
static char command_buffer[48];
static size_t command_length;
static UART_HandleTypeDef *console_uart;

static UART_HandleTypeDef *ui_uart(void) {
  return display_uart_handle();
}

static void handle_command(const char *line) {
  ui_cmd_t cmd;
  signal_gen_config_t next = *signal_gen_current();
  const signal_measure_result_t *measurement = signal_measure_latest();

  if (!ui_cmd_parse(line, &cmd)) {
    display_printf("ERR unknown command: %s\r\n", line);
    display_help();
    return;
  }

  switch (cmd.kind) {
    case UI_CMD_HELP:
      display_help();
      return;

    case UI_CMD_STATUS:
      display_status(signal_gen_current(), signal_measure_latest());
      return;

    case UI_CMD_SET_FREQ:
      next.frequency_hz = cmd.value;
      if (signal_gen_apply(&next)) {
        display_printf("OK freq=%lu\r\n", next.frequency_hz);
        display_status(signal_gen_current(), measurement);
      } else {
        display_printf("ERR freq range=%u..%u\r\n", APP_PWM_MIN_FREQ_HZ, APP_PWM_MAX_FREQ_HZ);
      }
      return;

    case UI_CMD_SET_DUTY:
      next.duty_percent = (uint8_t)cmd.value;
      if (signal_gen_apply(&next)) {
        display_printf("OK duty=%u\r\n", next.duty_percent);
        display_status(signal_gen_current(), measurement);
      } else {
        display_printf("ERR duty range=%u..%u\r\n", APP_PWM_MIN_DUTY_PERCENT, APP_PWM_MAX_DUTY_PERCENT);
      }
      return;

    case UI_CMD_NONE:
    case UI_CMD_INVALID:
    default:
      display_printf("ERR unknown command: %s\r\n", line);
      display_help();
      return;
  }
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

  bool line_ready = false;
  if (!ui_cmd_push_char(command_buffer, sizeof(command_buffer), ch, &command_length, &line_ready)) {
    display_write("ERR command too long\r\n");
    return;
  }

  if (line_ready) {
    handle_command(command_buffer);
  }
}
