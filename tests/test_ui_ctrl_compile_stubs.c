/*
 * Minimal stub definitions for compiling src/ui/ui_ctrl.c under HOST_TEST.
 * Provides safe no-op implementations of all HAL/board symbols that the
 * transitive includes pull in, so the test only verifies that ui_ctrl.c
 * compiles and links with HOST_TEST defined.
 *
 * hal_stubs.h is injected via -include compiler flag, not #included here.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "display/display.h"
#include "signal_gen/signal_gen_dac.h"
#include "signal_measure/signal_measure.h"
#include "touch/touch.h"
#include "ui/ui_cmd.h"
#include "ui/ui_ctrl.h"
#include "ui/ui_touch_map.h"
#include "ui/ui_actions.h"
#include "signal_capture/signal_capture_adc.h"
#include "scope/scope_render_logic.h"
#include "display/lcd_prim.h"
#include "display/display_lcd_scene.h"

/* Display stubs. */
void display_refresh_lcd(void) {}

/* Touch stubs. */
void touch_init(void) {}
void touch_poll(uint32_t now_ms) { (void)now_ms; }
bool touch_ready(void) { return false; }
const touch_runtime_t *touch_runtime(void) {
  static touch_runtime_t r = {0};
  r.state = TOUCH_STATE_INIT;
  return &r;
}
bool touch_pop_event(touch_event_t *event) {
  (void)event;
  return false;
}

/* Signal DAC stubs. */
const signal_gen_dac_status_t *signal_gen_dac_current(void) {
  static signal_gen_dac_status_t s = {
    .active = true,
    .frequency_hz = 1000U,
    .waveform = APP_DAC_WAVE_SQUARE,
    .ch_b_phase_offset_rad = 0.0f,
    .ch_b_frequency_ratio = 0U,
  };
  return &s;
}
bool signal_gen_dac_apply(const signal_gen_dac_config_t *config) {
  (void)config;
  return true;
}

/* Signal capture stubs. */
void signal_capture_adc_read_frame(scope_capture_frame_t *out, uint32_t now_ms) {
  (void)out;
  (void)now_ms;
}

/* UI action stubs. */
void ui_actions_init(ui_ctrl_view_t *view) { (void)view; }
void apply_pending_config(void) {}
void bump_config(int32_t freq_delta, int32_t duty_delta) {
  (void)freq_delta;
  (void)duty_delta;
}
void toggle_waveform(void) {}
void handle_ui_command(const ui_cmd_t *cmd) { (void)cmd; }

/* Touch map stubs. */
active_control_t hit_control(const touch_point_t *point, bool more_open) {
  (void)point;
  (void)more_open;
  return ACTIVE_NONE;
}
ui_highlight_t active_highlight_map(active_control_t control) {
  (void)control;
  return UI_HIGHLIGHT_NONE;
}

/* LCD scene/display stubs (called from ui_actions via display_refresh_lcd path). */
void display_lcd_scope_init(void) {}
void display_lcd_xy_init(void) {}
void lcd_scope_plot_refresh(const scope_capture_frame_t *frame) { (void)frame; }
void lcd_scope_info_refresh(const signal_gen_dac_status_t *dac,
                            const scope_capture_frame_t *frame) {
  (void)dac;
  (void)frame;
}
void lcd_xy_plot_refresh(const scope_capture_frame_t *frame, bool force_full) {
  (void)frame;
  (void)force_full;
}
void lcd_xy_info_refresh(const signal_gen_dac_status_t *dac) { (void)dac; }

/* HAL_GetTick implementation (definition after declaration above). */
static uint32_t _stub_tick = 0U;
uint32_t HAL_GetTick(void) {
  return _stub_tick++;
}

int main(void) {
  ui_ctrl_init();
  ui_ctrl_poll();
  ui_ctrl_poll();
  (void)ui_ctrl_view();
  return 0;
}
