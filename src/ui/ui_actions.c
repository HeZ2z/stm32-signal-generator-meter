#include "ui/ui_actions.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define _APP_PI_ 3.14159265358979f

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
  if (config.waveform != APP_DAC_WAVE_SQUARE &&
      config.waveform != APP_DAC_WAVE_TRIANGLE &&
      config.waveform != APP_DAC_WAVE_SINE) {
    config.waveform = APP_DAC_WAVE_SQUARE;
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
    const signal_gen_dac_status_t *dac = signal_gen_dac_current();
    actions_view->active_config.frequency_hz = dac->frequency_hz;
    actions_view->active_config.waveform = dac->waveform;
    actions_view->active_config.ch_b_phase_offset_rad = dac->ch_b_phase_offset_rad;
    actions_view->active_config.ch_b_frequency_ratio = dac->ch_b_frequency_ratio;
    actions_view->pending_config = actions_view->active_config;
  }
}

static bool apply_pending_config_with_footer(const char *success_text) {
  signal_gen_dac_config_t next;

  if (actions_view == NULL) {
    return false;
  }

  next = clamp_config(actions_view->pending_config);
  if (!signal_gen_dac_apply(&next)) {
    ui_sync_configs();
    ui_set_footer("APPLY FAILED");
    return false;
  }

  ui_sync_configs();
  if (success_text != NULL) {
    ui_set_footer(success_text);
  }
  return true;
}

void apply_pending_config(void) {
  if (actions_view == NULL) {
    return;
  }
  if (!apply_pending_config_with_footer(NULL)) {
    return;
  }

  (void)snprintf(actions_view->footer,
                 sizeof(actions_view->footer),
                 "APPLIED %s %luHZ",
                 signal_gen_dac_waveform_short_name(actions_view->active_config.waveform),
                 (unsigned long)actions_view->active_config.frequency_hz);
  display_status();
}

void bump_config(int32_t freq_delta, int32_t duty_delta) {
  if (actions_view == NULL) {
    return;
  }
  signal_gen_dac_config_t next = actions_view->pending_config;

  if (duty_delta != 0) {
    ui_set_footer("DAC DUTY FIXED BY WAVEFORM");
    display_refresh_lcd();
    return;
  }

  next.frequency_hz = (uint32_t)((int32_t)next.frequency_hz + freq_delta);
  actions_view->pending_config = clamp_config(next);
  apply_pending_config();
}

void toggle_waveform(void) {
  if (actions_view == NULL) {
    return;
  }

  if (actions_view->pending_config.waveform == APP_DAC_WAVE_SQUARE) {
    actions_view->pending_config.waveform = APP_DAC_WAVE_TRIANGLE;
  } else if (actions_view->pending_config.waveform == APP_DAC_WAVE_TRIANGLE) {
    actions_view->pending_config.waveform = APP_DAC_WAVE_SINE;
  } else {
    actions_view->pending_config.waveform = APP_DAC_WAVE_SQUARE;
  }

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
      display_write("INFO M10 waveform duty is not user-adjustable\r\n");
      ui_set_footer("DAC DUTY NOT ADJUSTABLE");
      display_refresh_lcd();
      break;
    case UI_CMD_SET_PHASE: {
      float phase_rad = (float)cmd->value / 100.0f;
      float clamped = phase_rad;
      if (clamped < 0.0f) {
        clamped = 0.0f;
      } else if (clamped > (2.0f * _APP_PI_)) {
        clamped = 2.0f * _APP_PI_;
      }
      actions_view->pending_config.ch_b_phase_offset_rad = clamped;
      if (apply_pending_config_with_footer("UART SET PHASE OK")) {
        display_status();
      }
      break;
    }
    case UI_CMD_SET_RATIO: {
      uint8_t ratio = (uint8_t)cmd->value;
      if (ratio == 0U) {
        ratio = 1U;
      }
      if (ratio > 9U) {
        ratio = 9U;
      }
      actions_view->pending_config.ch_b_frequency_ratio = ratio;
      if (apply_pending_config_with_footer("UART SET RATIO OK")) {
        display_status();
      }
      break;
    }
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
