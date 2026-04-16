#include "ui/ui_ctrl.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "display/display.h"
#include "main.h"
#include "signal_gen/signal_gen.h"
#include "signal_measure/signal_measure.h"
#include "touch/touch.h"
#include "ui/ui_cmd.h"

/* 保留一份行缓冲区，避免命令处理逻辑直接面对逐字节串口输入。 */
static char command_buffer[48];
static size_t command_length;
static UART_HandleTypeDef *console_uart;
static ui_ctrl_view_t view;

typedef enum {
  ACTIVE_NONE = 0,
  ACTIVE_FREQ_M1000,
  ACTIVE_FREQ_P1000,
  ACTIVE_DUTY_M5,
  ACTIVE_DUTY_P5,
  ACTIVE_RESET,
  ACTIVE_HELP,
} active_control_t;

typedef struct {
  uint16_t x0;
  uint16_t y0;
  uint16_t x1;
  uint16_t y1;
} rect_t;

static active_control_t active_control;

static ui_highlight_t active_highlight(active_control_t control) {
  switch (control) {
    case ACTIVE_FREQ_M1000:
      return UI_HIGHLIGHT_FREQ_DOWN;
    case ACTIVE_FREQ_P1000:
      return UI_HIGHLIGHT_FREQ_UP;
    case ACTIVE_DUTY_M5:
      return UI_HIGHLIGHT_DUTY_DOWN;
    case ACTIVE_DUTY_P5:
      return UI_HIGHLIGHT_DUTY_UP;
    case ACTIVE_RESET:
      return UI_HIGHLIGHT_RESET;
    case ACTIVE_HELP:
      return UI_HIGHLIGHT_HELP;
    case ACTIVE_NONE:
    default:
      return UI_HIGHLIGHT_NONE;
  }
}

static bool point_in_rect(const touch_point_t *point, rect_t rect) {
  return point->x >= rect.x0 && point->x <= rect.x1 && point->y >= rect.y0 && point->y <= rect.y1;
}

static signal_gen_config_t clamp_config(signal_gen_config_t config) {
  if (config.frequency_hz < APP_PWM_MIN_FREQ_HZ) {
    config.frequency_hz = APP_PWM_MIN_FREQ_HZ;
  } else if (config.frequency_hz > APP_PWM_MAX_FREQ_HZ) {
    config.frequency_hz = APP_PWM_MAX_FREQ_HZ;
  }

  if (config.duty_percent < APP_PWM_MIN_DUTY_PERCENT) {
    config.duty_percent = APP_PWM_MIN_DUTY_PERCENT;
  } else if (config.duty_percent > APP_PWM_MAX_DUTY_PERCENT) {
    config.duty_percent = APP_PWM_MAX_DUTY_PERCENT;
  }

  return config;
}

static void ui_set_footer(const char *text) {
  (void)snprintf(view.footer, sizeof(view.footer), "%s", text);
}

static void ui_sync_configs(void) {
  view.active_config = *signal_gen_current();
  view.pending_config = view.active_config;
}

static void ui_sync_touch_runtime(void) {
  const touch_runtime_t *runtime = touch_runtime();

  view.touch_ready = touch_ready();
  view.touch_pressed = runtime->pressed;
  view.highlight = active_highlight(active_control);
  if (runtime->last_point.x != 0U || runtime->last_point.y != 0U) {
    view.last_touch = runtime->last_point;
  }
  view.screen = UI_SCREEN_CONTROL;
  (void)snprintf(view.title,
                 sizeof(view.title),
                 "%s",
                 "HEZZZ/STM32-SIGNAL-GENERATOR");
}

static void ui_refresh_lcd(void) {
  display_refresh_lcd(signal_gen_current(), signal_measure_latest());
}

static void ui_toggle_more(bool open) {
  view.more_open = open;
  ui_set_footer(open ? "PROJECT INFO OPEN" : "PROJECT INFO CLOSED");
  ui_refresh_lcd();
}

/* UI 控制统一复用主显示模块选定的串口句柄。 */
static UART_HandleTypeDef *ui_uart(void) {
  return display_uart_handle();
}

static active_control_t hit_control(const touch_point_t *point) {
  if (view.more_open) {
    if (point_in_rect(point, (rect_t){360U, 14U, 458U, 42U})) {
      return ACTIVE_HELP;
    }
    return ACTIVE_NONE;
  }

  if (point_in_rect(point, (rect_t){22U, 202U, 106U, 246U})) {
    return ACTIVE_FREQ_M1000;
  }
  if (point_in_rect(point, (rect_t){110U, 202U, 194U, 246U})) {
    return ACTIVE_FREQ_P1000;
  }
  if (point_in_rect(point, (rect_t){198U, 202U, 282U, 246U})) {
    return ACTIVE_DUTY_M5;
  }
  if (point_in_rect(point, (rect_t){286U, 202U, 370U, 246U})) {
    return ACTIVE_DUTY_P5;
  }
  if (point_in_rect(point, (rect_t){374U, 202U, 458U, 246U})) {
    return ACTIVE_RESET;
  }
  if (point_in_rect(point, (rect_t){360U, 14U, 458U, 42U})) {
    return ACTIVE_HELP;
  }
  return ACTIVE_NONE;
}

static void apply_pending_config(void) {
  signal_gen_config_t next = clamp_config(view.pending_config);

  if (!signal_gen_apply(&next)) {
    ui_sync_configs();
    ui_set_footer("APPLY FAILED");
    return;
  }

  ui_sync_configs();
  (void)snprintf(view.footer,
                 sizeof(view.footer),
                 "APPLIED F=%luHZ D=%u%%",
                 view.active_config.frequency_hz,
                 view.active_config.duty_percent);
  display_status(signal_gen_current(), signal_measure_latest());
}

static void bump_config(int32_t freq_delta, int32_t duty_delta) {
  signal_gen_config_t next = view.pending_config;

  next.frequency_hz = (uint32_t)((int32_t)next.frequency_hz + freq_delta);
  next.duty_percent = (uint8_t)((int32_t)next.duty_percent + duty_delta);
  view.pending_config = clamp_config(next);
  apply_pending_config();
}

static void handle_ui_command(const ui_cmd_t *cmd) {
  signal_gen_config_t next = *signal_gen_current();

  switch (cmd->kind) {
    case UI_CMD_HELP:
      display_help();
      ui_set_footer("HELP PRINTED TO UART");
      break;
    case UI_CMD_STATUS:
      display_status(signal_gen_current(), signal_measure_latest());
      ui_set_footer("STATUS PRINTED TO UART");
      break;
    case UI_CMD_SET_FREQ:
      next.frequency_hz = cmd->value;
      if (!signal_gen_apply(&next)) {
        ui_set_footer("FREQ OUT OF RANGE");
        break;
      }
      ui_sync_configs();
      ui_set_footer("UART SET FREQ OK");
      display_status(signal_gen_current(), signal_measure_latest());
      break;
    case UI_CMD_SET_DUTY:
      next.duty_percent = (uint8_t)cmd->value;
      if (!signal_gen_apply(&next)) {
        ui_set_footer("DUTY OUT OF RANGE");
        break;
      }
      ui_sync_configs();
      ui_set_footer("UART SET DUTY OK");
      display_status(signal_gen_current(), signal_measure_latest());
      break;
    case UI_CMD_INVALID:
      display_write("ERR unknown command\r\n");
      display_help();
      ui_set_footer("UART CMD INVALID");
      break;
    case UI_CMD_NONE:
    default:
      break;
  }
}

static void handle_touch_event(const touch_event_t *event) {
  view.last_touch = event->point;
  ui_sync_touch_runtime();
  (void)snprintf(view.footer,
                 sizeof(view.footer),
                 "TOUCH X=%u Y=%u",
                 event->point.x,
                 event->point.y);

  switch (event->kind) {
    case TOUCH_EVENT_DOWN:
      active_control = hit_control(&event->point);
      view.highlight = active_highlight(active_control);
      ui_refresh_lcd();
      break;

    case TOUCH_EVENT_MOVE:
      active_control = hit_control(&event->point);
      view.highlight = active_highlight(active_control);
      ui_refresh_lcd();
      break;

    case TOUCH_EVENT_UP:
      switch (active_control) {
        case ACTIVE_FREQ_M1000:
          bump_config(-1000, 0);
          break;
        case ACTIVE_FREQ_P1000:
          bump_config(1000, 0);
          break;
        case ACTIVE_DUTY_M5:
          bump_config(0, -5);
          break;
        case ACTIVE_DUTY_P5:
          bump_config(0, 5);
          break;
        case ACTIVE_RESET:
          view.pending_config.frequency_hz = APP_DEFAULT_PWM_FREQ_HZ;
          view.pending_config.duty_percent = APP_DEFAULT_PWM_DUTY_PERCENT;
          apply_pending_config();
          break;
        case ACTIVE_HELP:
          ui_toggle_more(!view.more_open);
          break;
        case ACTIVE_NONE:
          if (view.more_open) {
            ui_toggle_more(false);
          }
          break;
        default:
          break;
      }
      active_control = ACTIVE_NONE;
      view.highlight = UI_HIGHLIGHT_NONE;
      ui_sync_touch_runtime();
      break;

    case TOUCH_EVENT_NONE:
    default:
      break;
  }
}

/* 复位命令缓冲状态并绑定控制串口。 */
void ui_ctrl_init(void) {
  console_uart = ui_uart();
  command_length = 0U;
  active_control = ACTIVE_NONE;
  (void)memset(&view, 0, sizeof(view));
  ui_sync_configs();
  touch_init();
  ui_sync_touch_runtime();
  if (touch_ready()) {
    ui_set_footer("TOUCH READY");
  } else {
    ui_set_footer(touch_runtime()->status);
  }
}

void ui_ctrl_poll(void) {
  uint8_t ch;
  uint32_t now = HAL_GetTick();
  touch_event_t event;
  ui_cmd_t cmd = {0};
  bool line_ready = false;

  touch_poll(now);
  ui_sync_touch_runtime();

  while (touch_pop_event(&event)) {
    handle_touch_event(&event);
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
    ui_set_footer("UART CMD TOO LONG");
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
