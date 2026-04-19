#ifndef DISPLAY_LCD_SCOPE_H
#define DISPLAY_LCD_SCOPE_H

#include <stdbool.h>
#include <stdint.h>

#include "signal_gen/signal_gen.h"
#include "signal_measure/signal_measure.h"
#include "signal_capture/signal_capture_adc.h"
#include "scope/scope_render_logic.h"
#include "ui/ui_ctrl.h"

typedef struct {
  bool valid;
  scope_render_trace_t trace;
  bool no_signal;
  scope_square_wave_estimate_t estimate;
  uint32_t estimate_ms;
  uint32_t estimate_config_hz;
  uint8_t estimate_miss_count;
} lcd_scope_state_t;

extern lcd_scope_state_t lcd_scope_state;

void display_lcd_scope_init(void);

uint32_t lcd_scope_adc_sample_rate_hz(void);
void lcd_draw_scope_static_plot_frame(void);
void lcd_restore_scope_columns(uint16_t x_begin, uint16_t x_end);
void lcd_draw_scope_trace(void);
void lcd_draw_info_cards(const signal_gen_config_t *config,
                         const signal_measure_result_t *measurement,
                         const scope_capture_snapshot_t *snapshot);
void lcd_draw_control_dynamic(const ui_ctrl_view_t *ui,
                              const signal_measure_result_t *measurement);

void lcd_draw_string(uint16_t x, uint16_t y, const char *text,
                     uint16_t fg, uint16_t bg, uint8_t scale);

#endif
