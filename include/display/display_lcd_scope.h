#ifndef DISPLAY_LCD_SCOPE_H
#define DISPLAY_LCD_SCOPE_H

#include <stdbool.h>
#include <stdint.h>

#include "signal_gen/signal_gen_dac.h"
#include "signal_measure/signal_measure.h"
#include "signal_capture/signal_capture_adc.h"
#include "scope/scope_render_logic.h"
#include "ui/ui_ctrl.h"

#ifndef APP_SCOPE_DUAL_LAYOUT
#define APP_SCOPE_DUAL_LAYOUT 0U
#endif

typedef struct {
  bool valid;
  scope_render_trace_t trace_a;
  scope_render_trace_t trace_b;
  bool no_signal_a;
  bool no_signal_b;
  scope_square_wave_estimate_t estimate_a;
  scope_square_wave_estimate_t estimate_b;
} lcd_scope_state_t;

extern lcd_scope_state_t lcd_scope_state;

void display_lcd_scope_init(void);

uint32_t lcd_scope_adc_sample_rate_hz(void);
void lcd_draw_scope_static_plot_frame(void);
void lcd_restore_scope_columns(uint16_t x_begin, uint16_t x_end);
void lcd_draw_control_dynamic(const ui_ctrl_view_t *ui,
                              const signal_measure_result_t *measurement);

void lcd_draw_string(uint16_t x, uint16_t y, const char *text,
                     uint16_t fg, uint16_t bg, uint8_t scale);

#endif
