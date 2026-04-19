#include "ui/ui_actions.h"

#include <stdio.h>
#include <string.h>

#include "board/board_config.h"
#include "display/display.h"
#include "signal_gen/signal_gen_dac.h"

static ui_ctrl_view_t *actions_view;

void ui_actions_init(ui_ctrl_view_t *view) {
  actions_view = view;
}

signal_gen_dac_config_t clamp_config(signal_gen_dac_config_t config) {
  if (config.frequency_hz < APP_DAC_MIN_FREQ_HZ) {
    config.frequency_hz = APP_DAC_MIN_FREQ_HZ;
  } else if (config.frequency_hz > APP_DAC_MAX_FREQ_HZ) {
    config.frequency_hz = APP_DAC_MAX_FREQ_HZ;
  }
  /* M9 当前只支持固定方波输出。 */
  config.waveform = APP_DAC_WAVE_SQUARE;
  return config;
}

static void ui_set_footer(const char *text) {
  if (actions_view != NULL) {
    (void)snprintf(actions_view->footer, sizeof(actions_view->footer), "%s", text);
  }
}

static void ui_sync_configs(void) {
  if (actions_view != NULL) {
    const signal_gen_dac_status_t *dac = signal_gen_dac_current();
    actions_view->active_config.frequency_hz = dac->frequency_hz;
    actions_view->active_config.waveform = dac->waveform;
    actions_view->pending_config = actions_view->active_config;
  }
}

void apply_pending_config(void) {
  if (actions_view == NULL) {
    return;
  }
  signal_gen_dac_config_t next = clamp_config(actions_view->pending_config);

  if (!signal_gen_dac_apply(&next)) {
    ui_sync_configs();
    ui_set_footer("APPLY FAILED");
    return;
  }

  ui_sync_configs();
  /* M9 当前固定 50% 占空比，因此 footer 直接写死该值。 */
  (void)snprintf(actions_view->footer,
                 sizeof(actions_view->footer),
                 "APPLIED DAC %luHZ 50%%",
                 (unsigned long)actions_view->active_config.frequency_hz);
  display_status();
}

void bump_config(int32_t freq_delta, int32_t duty_delta) {
  if (actions_view == NULL) {
    return;
  }
  signal_gen_dac_config_t next = actions_view->pending_config;

  if (duty_delta != 0) {
    ui_set_footer("M9 DAC DUTY FIXED 50%");
    display_refresh_lcd();
    return;
  }

  next.frequency_hz = (uint32_t)((int32_t)next.frequency_hz + freq_delta);
  actions_view->pending_config = clamp_config(next);
  apply_pending_config();
}

void handle_ui_command(const ui_cmd_t *cmd) {
  if (actions_view == NULL) {
    return;
  }
  signal_gen_dac_config_t next = actions_view->active_config;

  switch (cmd->kind) {
    case UI_CMD_HELP:
      display_help();
      ui_set_footer("HELP PRINTED TO UART");
      break;
    case UI_CMD_STATUS:
      display_status();
      ui_set_footer("STATUS PRINTED TO UART");
      break;
    case UI_CMD_SET_FREQ:
      next.frequency_hz = cmd->value;
      next.waveform = APP_DAC_WAVE_SQUARE;
      if (!signal_gen_dac_apply(&next)) {
        ui_set_footer("FREQ OUT OF RANGE");
        break;
      }
      ui_sync_configs();
      ui_set_footer("UART SET FREQ OK");
      display_status();
      break;
    case UI_CMD_SET_DUTY:
      (void)cmd->value;
      display_write("INFO M9 DAC square fixed at 50% duty\r\n");
      ui_set_footer("DAC DUTY FIXED 50%");
      display_refresh_lcd();
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
