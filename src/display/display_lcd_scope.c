#include "display/display_lcd_scope.h"
#include "display/display_lcd_scene.h"

#include <stdio.h>
#include <string.h>

#include "board/board_config.h"
#include "display/lcd_prim.h"
#include "signal_capture/signal_capture_adc.h"
#include "signal_gen/signal_gen_dac.h"

lcd_scope_state_t lcd_scope_state;

typedef enum {
  LCD_SCOPE_CARD_NO_SIGNAL = 0,
  LCD_SCOPE_CARD_ESTIMABLE,
  LCD_SCOPE_CARD_OUT_OF_WINDOW,
} lcd_scope_card_state_t;

typedef struct {
  lcd_scope_card_state_t state;
  uint32_t frequency_hz;
  uint8_t duty_percent;
} lcd_scope_card_status_t;

static const char *waveform_short_name(dac_waveform_t waveform) {
  switch (waveform) {
    case APP_DAC_WAVE_TRIANGLE:
      return "TRI";
    case APP_DAC_WAVE_SQUARE:
      return "SQR";
    case APP_DAC_WAVE_SINE:
    default:
      return "UNK";
  }
}

static bool build_trace_from_snapshot(const scope_capture_snapshot_t *snapshot,
                                      scope_render_trace_t *trace) {
  if (snapshot == NULL || !snapshot->valid) {
    return false;
  }

  return scope_render_build_trace(snapshot->samples, snapshot->sample_count,
                                  LCD_SCOPE_PLOT_HEIGHT,
                                  APP_SCOPE_SIGNAL_FLAT_THRESHOLD, trace) &&
         !trace->flat_signal;
}

static void draw_trace(const scope_render_trace_t *trace, uint16_t color) {
  if (trace == NULL || !trace->valid || trace->point_count < 2U) {
    return;
  }

  for (uint16_t i = 1U; i < trace->point_count; ++i) {
    uint16_t x0 = (uint16_t)(LCD_SCOPE_PLOT_X +
                  (((uint32_t)(i - 1U) * (LCD_SCOPE_PLOT_WIDTH - 1U)) /
                   (trace->point_count - 1U)));
    uint16_t x1 = (uint16_t)(LCD_SCOPE_PLOT_X +
                  (((uint32_t)i * (LCD_SCOPE_PLOT_WIDTH - 1U)) /
                   (trace->point_count - 1U)));
    uint16_t y0 = (uint16_t)(LCD_SCOPE_PLOT_Y + trace->y_points[i - 1U]);
    uint16_t y1 = (uint16_t)(LCD_SCOPE_PLOT_Y + trace->y_points[i]);
    lcd_draw_line(x0, y0, x1, y1, color);
  }
}

static lcd_scope_card_status_t build_card_status(
    const scope_capture_snapshot_t *snapshot,
    dac_waveform_t waveform,
    uint32_t config_hz) {
  lcd_scope_card_status_t status = {
      .state = LCD_SCOPE_CARD_NO_SIGNAL,
      .frequency_hz = 0U,
      .duty_percent = 0U,
  };
  scope_square_wave_estimate_t estimate = {0};
  uint32_t sample_rate_hz = lcd_scope_adc_sample_rate_hz();
  uint32_t min_frequency_hz = 0U;
  uint32_t max_frequency_hz = 0U;
  bool within_window = false;

  if (snapshot == NULL || !snapshot->valid) {
    return status;
  }

  if (waveform == APP_DAC_WAVE_TRIANGLE) {
    status.state = LCD_SCOPE_CARD_OUT_OF_WINDOW;
    status.frequency_hz = config_hz;
    return status;
  }

  within_window = scope_square_wave_frequency_window(snapshot->sample_count,
                                                     sample_rate_hz,
                                                     &min_frequency_hz,
                                                     &max_frequency_hz);
  if (!within_window) {
    status.state = LCD_SCOPE_CARD_OUT_OF_WINDOW;
    status.frequency_hz = config_hz;
    return status;
  }

  if (!scope_square_wave_signal_present(snapshot->samples, snapshot->sample_count,
                                        APP_SCOPE_SIGNAL_FLAT_THRESHOLD,
                                        APP_SCOPE_SIGNAL_MIN_SPAN,
                                        APP_SCOPE_SQUARE_LOW_LEVEL_MAX,
                                        APP_SCOPE_SQUARE_HIGH_LEVEL_MIN)) {
    return status;
  }

  if (!scope_estimate_square_wave(snapshot->samples, snapshot->sample_count,
                                  APP_SCOPE_SIGNAL_FLAT_THRESHOLD,
                                  sample_rate_hz, &estimate)) {
    return status;
  }

  status.state = LCD_SCOPE_CARD_ESTIMABLE;
  status.frequency_hz = estimate.frequency_hz;
  status.duty_percent = estimate.duty_percent;
  return status;
}

void display_lcd_scope_init(void) {
  (void)memset(&lcd_scope_state, 0, sizeof(lcd_scope_state));
}

uint32_t lcd_scope_adc_sample_rate_hz(void) {
  return signal_capture_adc_channel_sample_rate_hz();
}

void lcd_draw_scope_static_plot_frame(void) {
  uint16_t bg = lcd_rgb565(10, 20, 42);
  uint16_t grid = lcd_rgb565(32, 66, 108);

  lcd_fill_rect(LCD_SCOPE_PLOT_X, LCD_SCOPE_PLOT_Y, LCD_SCOPE_PLOT_WIDTH,
                LCD_SCOPE_PLOT_HEIGHT, bg);
  lcd_draw_box(LCD_SCOPE_PLOT_X, LCD_SCOPE_PLOT_Y, LCD_SCOPE_PLOT_WIDTH,
               LCD_SCOPE_PLOT_HEIGHT, grid, bg);
  lcd_draw_hline(LCD_SCOPE_PLOT_X,
                 (uint16_t)(LCD_SCOPE_PLOT_Y + (LCD_SCOPE_PLOT_HEIGHT / 2U)),
                 LCD_SCOPE_PLOT_WIDTH, grid);
  for (uint16_t x = (uint16_t)(LCD_SCOPE_PLOT_X + 53U);
       x < (LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH);
       x = (uint16_t)(x + 53U)) {
    lcd_draw_vline(x, LCD_SCOPE_PLOT_Y, LCD_SCOPE_PLOT_HEIGHT, grid);
  }
}

void lcd_restore_scope_columns(uint16_t x_begin, uint16_t x_end) {
  uint16_t bg = lcd_rgb565(10, 20, 42);
  uint16_t grid = lcd_rgb565(32, 66, 108);
  uint16_t start = x_begin < LCD_SCOPE_PLOT_X ? LCD_SCOPE_PLOT_X : x_begin;
  uint16_t end = x_end >= (LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH)
                     ? (uint16_t)(LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH - 1U)
                     : x_end;
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

static void lcd_draw_scope_trace(const scope_capture_frame_t *frame) {
  scope_render_trace_t trace_a = {0};
  scope_render_trace_t trace_b = {0};
  uint16_t warn = lcd_rgb565(255, 208, 116);
  uint16_t trace_a_color = lcd_rgb565(112, 232, 162);
  uint16_t trace_b_color = lcd_rgb565(255, 166, 78);
  uint16_t x_begin = (uint16_t)(LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH - 1U);
  uint16_t x_end = LCD_SCOPE_PLOT_X;
  bool has_a = build_trace_from_snapshot(frame != NULL ? &frame->ch_a : NULL, &trace_a);
  bool has_b = build_trace_from_snapshot(frame != NULL ? &frame->ch_b : NULL, &trace_b);

  if (!has_a && !has_b) {
    lcd_draw_scope_static_plot_frame();
    lcd_draw_string((uint16_t)(LCD_SCOPE_PLOT_X + 74U),
                    (uint16_t)(LCD_SCOPE_PLOT_Y + 36U),
                    "NO SIGNAL CH-A / CH-B", warn, lcd_rgb565(10, 20, 42), 2);
    lcd_scope_state.valid = false;
    lcd_scope_state.no_signal_a = true;
    lcd_scope_state.no_signal_b = true;
    return;
  }

  if (lcd_scope_state.valid) {
    for (uint16_t i = 0U; i < APP_SCOPE_SAMPLE_COUNT; ++i) {
      bool changed = false;

      if (has_a && (i >= lcd_scope_state.trace_a.point_count ||
                    lcd_scope_state.trace_a.y_points[i] != trace_a.y_points[i])) {
        changed = true;
      }
      if (has_b && (i >= lcd_scope_state.trace_b.point_count ||
                    lcd_scope_state.trace_b.y_points[i] != trace_b.y_points[i])) {
        changed = true;
      }

      if (changed) {
        uint16_t x = (uint16_t)(LCD_SCOPE_PLOT_X +
                     (((uint32_t)i * (LCD_SCOPE_PLOT_WIDTH - 1U)) /
                      (APP_SCOPE_SAMPLE_COUNT - 1U)));
        if (x < x_begin) {
          x_begin = x;
        }
        if (x > x_end) {
          x_end = x;
        }
      }
    }
  }

  if (!lcd_scope_state.valid || x_end < x_begin) {
    lcd_draw_scope_static_plot_frame();
  } else {
    x_begin = x_begin > (LCD_SCOPE_PLOT_X + 2U) ? (uint16_t)(x_begin - 2U)
                                                : LCD_SCOPE_PLOT_X;
    x_end = (uint16_t)(x_end + 2U);
    lcd_restore_scope_columns(x_begin, x_end);
  }

  if (has_a) {
    draw_trace(&trace_a, trace_a_color);
  }
  if (has_b) {
    draw_trace(&trace_b, trace_b_color);
  }

  lcd_scope_state.trace_a = trace_a;
  lcd_scope_state.trace_b = trace_b;
  lcd_scope_state.no_signal_a = !has_a;
  lcd_scope_state.no_signal_b = !has_b;
  lcd_scope_state.valid = true;
}

static void lcd_draw_info_cards(const signal_gen_dac_status_t *dac,
                                const scope_capture_frame_t *frame) {
  uint16_t panel = lcd_rgb565(7, 16, 34);
  uint16_t border = lcd_rgb565(54, 122, 178);
  uint16_t input_a = lcd_rgb565(112, 232, 162);
  uint16_t input_b = lcd_rgb565(255, 166, 78);
  uint16_t output_accent = lcd_rgb565(255, 182, 86);
  uint16_t text = lcd_rgb565(236, 246, 255);
  uint16_t warn = lcd_rgb565(255, 208, 116);
  char line[48];
  lcd_scope_card_status_t status_a =
      build_card_status(frame != NULL ? &frame->ch_a : NULL, dac->waveform,
                        dac->frequency_hz);
  lcd_scope_card_status_t status_b =
      build_card_status(frame != NULL ? &frame->ch_b : NULL, dac->waveform,
                        dac->frequency_hz);

  lcd_scope_state.estimate_a.valid =
      status_a.state == LCD_SCOPE_CARD_ESTIMABLE;
  lcd_scope_state.estimate_a.frequency_hz = status_a.frequency_hz;
  lcd_scope_state.estimate_a.duty_percent = status_a.duty_percent;
  lcd_scope_state.estimate_b.valid =
      status_b.state == LCD_SCOPE_CARD_ESTIMABLE;
  lcd_scope_state.estimate_b.frequency_hz = status_b.frequency_hz;
  lcd_scope_state.estimate_b.duty_percent = status_b.duty_percent;

  lcd_draw_card(18, 14, 198, 38, border, panel, input_a);
  lcd_draw_card(222, 14, 116, 38, border, panel, output_accent);

  lcd_draw_string(26, 20, "INPUT", text, panel, 1);
  lcd_draw_string(26, 30, "CH-A PA4->PA0", input_a, panel, 1);
  if (status_a.state == LCD_SCOPE_CARD_ESTIMABLE) {
    (void)snprintf(line, sizeof(line), "F=%luHZ D=%u%%",
                   (unsigned long)status_a.frequency_hz,
                   status_a.duty_percent);
    lcd_draw_string(116, 30, line, text, panel, 1);
  } else if (status_a.state == LCD_SCOPE_CARD_OUT_OF_WINDOW) {
    (void)snprintf(line, sizeof(line), "F=%luHZ D=--",
                   (unsigned long)status_a.frequency_hz);
    lcd_draw_string(116, 30, line, text, panel, 1);
  } else {
    lcd_draw_string(116, 30, "NO SIGNAL", warn, panel, 1);
  }

  lcd_draw_string(26, 40, "CH-B PA5->PA6", input_b, panel, 1);
  if (status_b.state == LCD_SCOPE_CARD_ESTIMABLE) {
    (void)snprintf(line, sizeof(line), "F=%luHZ D=%u%%",
                   (unsigned long)status_b.frequency_hz,
                   status_b.duty_percent);
    lcd_draw_string(116, 40, line, text, panel, 1);
  } else if (status_b.state == LCD_SCOPE_CARD_OUT_OF_WINDOW) {
    (void)snprintf(line, sizeof(line), "F=%luHZ D=--",
                   (unsigned long)status_b.frequency_hz);
    lcd_draw_string(116, 40, line, text, panel, 1);
  } else {
    lcd_draw_string(116, 40, "NO SIGNAL", warn, panel, 1);
  }

  lcd_draw_string(230, 20, "OUTPUT", text, panel, 1);
  lcd_draw_string(230, 30, "PA4/PA5 DAC1/2", output_accent, panel, 1);
  (void)snprintf(line, sizeof(line), "F=%luHZ %s",
                 (unsigned long)dac->frequency_hz,
                 waveform_short_name(dac->waveform));
  lcd_draw_string(230, 40, line, text, panel, 1);
}

void lcd_draw_control_dynamic(const ui_ctrl_view_t *ui,
                              const signal_measure_result_t *measurement) {
  scope_capture_frame_t frame = {0};
  uint32_t now = HAL_GetTick();

  (void)measurement;
  signal_capture_adc_read_frame(&frame, now);
  lcd_draw_scope_trace(&frame);
  lcd_draw_info_cards(signal_gen_dac_current(), &frame);
  lcd_draw_footer(ui);
}
