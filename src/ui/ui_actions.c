#include "ui/ui_actions.h"

#include <stdio.h>
#include <string.h>

#include "board/board_config.h"
#include "display/display.h"
#include "signal_gen/signal_gen.h"
#include "signal_measure/signal_measure.h"

static ui_ctrl_view_t *actions_view;

void ui_actions_init(ui_ctrl_view_t *view) {
  actions_view = view;
}

signal_gen_config_t clamp_config(signal_gen_config_t config) {
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
  if (actions_view != NULL) {
    (void)snprintf(actions_view->footer, sizeof(actions_view->footer), "%s", text);
  }
}

static void ui_sync_configs(void) {
  if (actions_view != NULL) {
    actions_view->active_config = *signal_gen_current();
    actions_view->pending_config = actions_view->active_config;
  }
}

void apply_pending_config(void) {
  if (actions_view == NULL) {
    return;
  }
  signal_gen_config_t next = clamp_config(actions_view->pending_config);

  if (!signal_gen_apply(&next)) {
    ui_sync_configs();
    ui_set_footer("APPLY FAILED");
    return;
  }

  ui_sync_configs();
  (void)snprintf(actions_view->footer,
                 sizeof(actions_view->footer),
                 "APPLIED F=%luHZ D=%u%%",
                 (unsigned long)actions_view->active_config.frequency_hz,
                 actions_view->active_config.duty_percent);
  display_status(signal_gen_current(), signal_measure_latest());
}

void bump_config(int32_t freq_delta, int32_t duty_delta) {
  if (actions_view == NULL) {
    return;
  }
  signal_gen_config_t next = actions_view->pending_config;

  next.frequency_hz = (uint32_t)((int32_t)next.frequency_hz + freq_delta);
  next.duty_percent = (uint8_t)((int32_t)next.duty_percent + duty_delta);
  actions_view->pending_config = clamp_config(next);
  apply_pending_config();
}

void handle_ui_command(const ui_cmd_t *cmd) {
  if (actions_view == NULL) {
    return;
  }
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
