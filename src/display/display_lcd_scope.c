#include "display/display_lcd_scope.h"
#include "display/display_lcd_scene.h"

#include <stdio.h>
#include <string.h>

#include "board/board_config.h"
#include "display/lcd_prim.h"
#include "signal_capture/signal_capture_adc.h"
#include "signal_measure/signal_measure.h"
#include "ui/ui_ctrl.h"

lcd_scope_state_t lcd_scope_state;

void display_lcd_scope_init(void) {
  (void)memset(&lcd_scope_state, 0, sizeof(lcd_scope_state));
}

uint32_t lcd_scope_adc_sample_rate_hz(void) {
  uint32_t pclk2_hz = HAL_RCC_GetPCLK2Freq();
  return pclk2_hz / 4U / 96U;
}

void lcd_draw_scope_static_plot_frame(void) {
  uint16_t bg = lcd_rgb565(10, 20, 42);
  uint16_t grid = lcd_rgb565(32, 66, 108);

  lcd_fill_rect(LCD_SCOPE_PLOT_X, LCD_SCOPE_PLOT_Y,
                LCD_SCOPE_PLOT_WIDTH, LCD_SCOPE_PLOT_HEIGHT, bg);
  lcd_draw_box(LCD_SCOPE_PLOT_X, LCD_SCOPE_PLOT_Y,
               LCD_SCOPE_PLOT_WIDTH, LCD_SCOPE_PLOT_HEIGHT, grid, bg);
  lcd_draw_hline(LCD_SCOPE_PLOT_X,
                  (uint16_t)(LCD_SCOPE_PLOT_Y + (LCD_SCOPE_PLOT_HEIGHT / 2U)),
                  LCD_SCOPE_PLOT_WIDTH, grid);
  for (uint16_t x = (uint16_t)(LCD_SCOPE_PLOT_X + 53U);
       x < (LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH); x = (uint16_t)(x + 53U)) {
    lcd_draw_vline(x, LCD_SCOPE_PLOT_Y, LCD_SCOPE_PLOT_HEIGHT, grid);
  }
}

void lcd_restore_scope_columns(uint16_t x_begin, uint16_t x_end) {
  uint16_t bg = lcd_rgb565(10, 20, 42);
  uint16_t grid = lcd_rgb565(32, 66, 108);
  uint16_t start = x_begin < LCD_SCOPE_PLOT_X ? LCD_SCOPE_PLOT_X : x_begin;
  uint16_t end = x_end >= (LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH) ?
                 (uint16_t)(LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH - 1U) : x_end;
  uint16_t width;

  if (end < start) {
    return;
  }

  width = (uint16_t)(end - start + 1U);
  lcd_fill_rect(start, LCD_SCOPE_PLOT_Y, width, LCD_SCOPE_PLOT_HEIGHT, bg);
  lcd_draw_hline(start,
                 (uint16_t)(LCD_SCOPE_PLOT_Y + (LCD_SCOPE_PLOT_HEIGHT / 2U)),
                 width, grid);
  for (uint16_t x = start; x <= end; ++x) {
    if (x == LCD_SCOPE_PLOT_X ||
        x == (uint16_t)(LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH - 1U) ||
        ((uint16_t)(x - LCD_SCOPE_PLOT_X) % 53U) == 0U) {
      lcd_draw_vline(x, LCD_SCOPE_PLOT_Y, LCD_SCOPE_PLOT_HEIGHT, grid);
    }
  }
}

void lcd_draw_scope_trace(void) {
  const scope_capture_snapshot_t *snapshot = signal_capture_adc_latest();
  scope_render_trace_t trace = {0};
  scope_render_trace_t last_trace = lcd_scope_state.trace;
  uint16_t trace_color = lcd_rgb565(112, 232, 162);
  uint16_t warn = lcd_rgb565(255, 208, 116);
  char label[32];
  bool no_signal = false;

  if (snapshot == NULL || !snapshot->valid ||
      !scope_render_build_trace(snapshot->samples,
                                snapshot->sample_count,
                                LCD_SCOPE_PLOT_HEIGHT,
                                APP_SCOPE_SIGNAL_FLAT_THRESHOLD,
                                &trace) ||
      trace.flat_signal) {
    no_signal = true;
  }

  if (no_signal) {
    if (!lcd_scope_state.no_signal) {
      lcd_draw_scope_static_plot_frame();
    }
    lcd_draw_string((uint16_t)(LCD_SCOPE_PLOT_X + 86U),
                    (uint16_t)(LCD_SCOPE_PLOT_Y + 36U),
                    "NO SIGNAL ON PA0", warn,
                    lcd_rgb565(10, 20, 42), 2);
    lcd_scope_state.valid = false;
    lcd_scope_state.no_signal = true;
    return;
  }

  if (!lcd_scope_state.valid || lcd_scope_state.no_signal) {
    lcd_draw_scope_static_plot_frame();
  } else {
    uint16_t first_changed = 0U;
    uint16_t last_changed = 0U;
    bool changed = false;

    for (uint16_t i = 0; i < trace.point_count; ++i) {
      if (i >= last_trace.point_count ||
          last_trace.y_points[i] != trace.y_points[i]) {
        if (!changed) {
          first_changed = i;
          changed = true;
        }
        last_changed = i;
      }
    }

    if (changed) {
      uint16_t x_begin = (uint16_t)(LCD_SCOPE_PLOT_X +
                        (((uint32_t)first_changed * (LCD_SCOPE_PLOT_WIDTH - 1U)) /
                         (trace.point_count - 1U)));
      uint16_t x_end = (uint16_t)(LCD_SCOPE_PLOT_X +
                      (((uint32_t)last_changed * (LCD_SCOPE_PLOT_WIDTH - 1U)) /
                       (trace.point_count - 1U)));
      x_begin = x_begin > (LCD_SCOPE_PLOT_X + 2U) ?
                (uint16_t)(x_begin - 2U) : LCD_SCOPE_PLOT_X;
      x_end = (uint16_t)(x_end + 2U);
      lcd_restore_scope_columns(x_begin, x_end);
    }
  }

  for (uint16_t i = 1; i < trace.point_count; ++i) {
    uint16_t x0 = (uint16_t)(LCD_SCOPE_PLOT_X +
                  (((uint32_t)(i - 1U) * (LCD_SCOPE_PLOT_WIDTH - 1U)) /
                   (trace.point_count - 1U)));
    uint16_t x1 = (uint16_t)(LCD_SCOPE_PLOT_X +
                  (((uint32_t)i * (LCD_SCOPE_PLOT_WIDTH - 1U)) /
                   (trace.point_count - 1U)));
    uint16_t y0 = (uint16_t)(LCD_SCOPE_PLOT_Y + trace.y_points[i - 1U]);
    uint16_t y1 = (uint16_t)(LCD_SCOPE_PLOT_Y + trace.y_points[i]);
    lcd_draw_line(x0, y0, x1, y1, trace_color);
  }

  (void)snprintf(label, sizeof(label), "N=%u", trace.point_count);
  lcd_draw_string((uint16_t)(LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH - 44U),
                  (uint16_t)(LCD_SCOPE_PLOT_Y + 4U),
                  label, lcd_rgb565(236, 246, 255),
                  lcd_rgb565(10, 20, 42), 1);
  lcd_scope_state.trace = trace;
  lcd_scope_state.valid = true;
  lcd_scope_state.no_signal = false;
}

void lcd_draw_info_cards(const signal_gen_config_t *config,
                         const signal_measure_result_t *measurement,
                         const scope_capture_snapshot_t *snapshot) {
  uint16_t panel = lcd_rgb565(7, 16, 34);
  uint16_t border = lcd_rgb565(54, 122, 178);
  uint16_t input_accent = lcd_rgb565(112, 232, 162);
  uint16_t output_accent = lcd_rgb565(255, 182, 86);
  uint16_t text = lcd_rgb565(236, 246, 255);
  uint16_t warn = lcd_rgb565(255, 208, 116);
  char line[40];
  scope_square_wave_estimate_t estimate = {0};
  uint32_t min_frequency_hz = 0U;
  uint32_t max_frequency_hz = 0U;
  uint32_t adc_sample_rate_hz = lcd_scope_adc_sample_rate_hz();
  uint32_t now_ms = HAL_GetTick();
  bool duty_frequency_window_valid = scope_square_wave_frequency_window(
      APP_SCOPE_SAMPLE_COUNT, adc_sample_rate_hz,
      &min_frequency_hz, &max_frequency_hz);
  bool within_duty_window = duty_frequency_window_valid &&
                            config->frequency_hz >= min_frequency_hz &&
                            config->frequency_hz <= max_frequency_hz;
  bool square_signal_present = snapshot != NULL && snapshot->valid &&
                               scope_square_wave_signal_present(
                                   snapshot->samples, snapshot->sample_count,
                                   APP_SCOPE_SIGNAL_FLAT_THRESHOLD,
                                   APP_SCOPE_SIGNAL_MIN_SPAN,
                                   APP_SCOPE_SQUARE_LOW_LEVEL_MAX,
                                   APP_SCOPE_SQUARE_HIGH_LEVEL_MIN);
  (void)measurement;

  if (square_signal_present && within_duty_window) {
    (void)scope_estimate_square_wave(snapshot->samples,
                                     snapshot->sample_count,
                                     APP_SCOPE_SIGNAL_FLAT_THRESHOLD,
                                     adc_sample_rate_hz, &estimate);
    if (estimate.valid) {
      lcd_scope_state.estimate = estimate;
      lcd_scope_state.estimate_ms = now_ms;
      lcd_scope_state.estimate_config_hz = config->frequency_hz;
      lcd_scope_state.estimate_miss_count = 0U;
    } else if (lcd_scope_state.estimate.valid &&
               lcd_scope_state.estimate_config_hz == config->frequency_hz &&
               lcd_scope_state.estimate_miss_count < 0xFFU) {
      ++lcd_scope_state.estimate_miss_count;
    }
  } else {
    lcd_scope_state.estimate.valid = false;
    lcd_scope_state.estimate_ms = 0U;
    lcd_scope_state.estimate_config_hz = 0U;
    lcd_scope_state.estimate_miss_count = 0U;
  }

  lcd_draw_card(18, 14, 156, 28, border, panel, input_accent);
  lcd_draw_card(182, 14, 156, 28, border, panel, output_accent);

  lcd_draw_string(26, 20, "INPUT", text, panel, 1);
  lcd_draw_string(68, 20, "PA0 ADC1", input_accent, panel, 1);
  if (square_signal_present) {
    if (lcd_scope_state.estimate.valid &&
        lcd_scope_state.estimate_config_hz == config->frequency_hz) {
      (void)snprintf(line, sizeof(line), "F=%luHZ D=%u%%",
                     (unsigned long)lcd_scope_state.estimate.frequency_hz,
                     lcd_scope_state.estimate.duty_percent);
      lcd_draw_string(26, 30, line, text, panel, 1);
    } else if (within_duty_window) {
      lcd_draw_string(26, 30, "F=ADC LIVE D=--", warn, panel, 1);
    } else {
      (void)snprintf(line, sizeof(line), "F=%luHZ D=--",
                     (unsigned long)config->frequency_hz);
      lcd_draw_string(26, 30, line, warn, panel, 1);
    }
  } else {
    lcd_draw_string(26, 30, "NO INPUT", warn, panel, 1);
  }

  lcd_draw_string(190, 20, "OUTPUT", text, panel, 1);
  lcd_draw_string(238, 20, "PB6 TIM4", output_accent, panel, 1);
  (void)snprintf(line, sizeof(line), "F=%luHZ D=%u%%",
                 (unsigned long)config->frequency_hz, config->duty_percent);
  lcd_draw_string(190, 30, line, text, panel, 1);
}

void lcd_draw_control_dynamic(const ui_ctrl_view_t *ui,
                              const signal_measure_result_t *measurement) {
  const scope_capture_snapshot_t *snapshot = signal_capture_adc_latest();
  lcd_draw_scope_trace();
  lcd_draw_info_cards(&ui->pending_config, measurement, snapshot);
  lcd_draw_footer(ui);
}
