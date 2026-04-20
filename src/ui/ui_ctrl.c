#include "ui/ui_ctrl.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "display/display.h"
#include "main.h"
#include "signal_gen/signal_gen_dac.h"
#include "touch/touch.h"
#include "ui/ui_actions.h"
#include "ui/ui_cmd.h"
#include "ui/ui_touch_map.h"

static char command_buffer[48];
static size_t command_length;
static UART_HandleTypeDef *console_uart;
static ui_ctrl_view_t view;
static active_control_t active_control;

void ui_ctrl_init(void) {
  console_uart = display_uart_handle();
  command_length = 0U;
  active_control = ACTIVE_NONE;
  (void)memset(&view, 0, sizeof(view));

  view.active_config.frequency_hz = signal_gen_dac_current()->frequency_hz;
  view.active_config.waveform = signal_gen_dac_current()->waveform;
  view.pending_config = view.active_config;
  (void)snprintf(view.title, sizeof(view.title), "HeZ2z/stm32-sig-meter");

  ui_actions_init(&view);
  touch_init();

  view.touch_ready = touch_ready();
  if (touch_ready()) {
    (void)snprintf(view.footer, sizeof(view.footer), "TOUCH READY");
  } else {
    (void)snprintf(view.footer, sizeof(view.footer), "%s", touch_runtime()->status);
  }
  view.screen = UI_SCREEN_CONTROL;
}

void ui_ctrl_poll(void) {
  uint8_t ch;
  uint32_t now = HAL_GetTick();
  touch_event_t event;
  ui_cmd_t cmd = {0};
  bool line_ready = false;

  touch_poll(now);

  view.touch_ready = touch_ready();
  if (view.touch_ready) {
    const touch_runtime_t *runtime = touch_runtime();
    view.touch_pressed = runtime->pressed;
    if (runtime->last_point.x != 0U || runtime->last_point.y != 0U) {
      view.last_touch = runtime->last_point;
    }
  }

  while (touch_pop_event(&event)) {
    view.last_touch = event.point;
    switch (event.kind) {
      case TOUCH_EVENT_DOWN:
      case TOUCH_EVENT_MOVE:
        active_control = hit_control(&event.point, view.more_open);
        view.highlight = active_highlight_map(active_control);
        display_refresh_lcd();
        break;
      case TOUCH_EVENT_UP:
        switch (active_control) {
          case ACTIVE_FREQ_M1000:
            bump_config(-1000, 0);
            break;
          case ACTIVE_FREQ_P1000:
            bump_config(1000, 0);
            break;
          case ACTIVE_WAVE_TOGGLE:
            toggle_waveform();
            break;
          case ACTIVE_SCREEN_TOGGLE:
            view.screen = view.screen == UI_SCREEN_XY ? UI_SCREEN_CONTROL
                                                      : UI_SCREEN_XY;
            (void)snprintf(view.footer, sizeof(view.footer),
                           view.screen == UI_SCREEN_XY ? "VIEW XY" : "VIEW YT");
            display_refresh_lcd();
            break;
          case ACTIVE_RESET:
            view.pending_config.frequency_hz = APP_DEFAULT_DAC_FREQ_HZ;
            view.pending_config.waveform = APP_DAC_WAVE_SQUARE;
            view.screen = UI_SCREEN_CONTROL;
            apply_pending_config();
            break;
          case ACTIVE_HELP:
            view.more_open = !view.more_open;
            (void)snprintf(view.footer, sizeof(view.footer),
                           view.more_open ? "PROJECT INFO OPEN" : "PROJECT INFO CLOSED");
            display_refresh_lcd();
            break;
          case ACTIVE_NONE:
            if (view.more_open) {
              view.more_open = false;
              (void)snprintf(view.footer, sizeof(view.footer), "PROJECT INFO CLOSED");
              display_refresh_lcd();
            }
            break;
          default:
            break;
        }
        active_control = ACTIVE_NONE;
        view.highlight = UI_HIGHLIGHT_NONE;
        break;
      case TOUCH_EVENT_NONE:
      default:
        break;
    }
  }

  if (HAL_UART_Receive(console_uart, &ch, 1U, 0U) != HAL_OK) {
    return;
  }

  if (!ui_cmd_push_char(command_buffer,
                        sizeof(command_buffer),
                        ch,
                        &command_length,
                        &line_ready)) {
    display_write("ERR command too long\r\n");
    (void)snprintf(view.footer, sizeof(view.footer), "UART CMD TOO LONG");
    return;
  }

  if (!line_ready) {
    return;
  }

  if (command_buffer[0] == '\0') {
    return;
  }

  if (!ui_cmd_parse(command_buffer, &cmd)) {
    cmd.kind = UI_CMD_INVALID;
  }
  handle_ui_command(&cmd);
}

const ui_ctrl_view_t *ui_ctrl_view(void) {
  return &view;
}
