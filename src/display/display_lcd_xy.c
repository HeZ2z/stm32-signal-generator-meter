#include "display/display_lcd_xy.h"

#include <stdio.h>

#include "display/display_lcd_scene.h"
#include "display/display_lcd_scope.h"
#include "display/lcd_prim.h"
#include "signal_capture/signal_capture_adc.h"
#include "signal_gen/signal_gen_dac.h"

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

static uint16_t xy_map_x(uint16_t raw) {
  return (uint16_t)(LCD_SCOPE_PLOT_X +
                    (((uint32_t)raw * (LCD_SCOPE_PLOT_WIDTH - 1U)) / 4095U));
}

static uint16_t xy_map_y(uint16_t raw) {
  return (uint16_t)(LCD_SCOPE_PLOT_Y + (LCD_SCOPE_PLOT_HEIGHT - 1U) -
                    (((uint32_t)raw * (LCD_SCOPE_PLOT_HEIGHT - 1U)) / 4095U));
}

static void lcd_draw_xy_frame(void) {
  uint16_t bg = lcd_rgb565(10, 20, 42);
  uint16_t grid = lcd_rgb565(32, 66, 108);

  lcd_fill_rect(LCD_SCOPE_PLOT_X, LCD_SCOPE_PLOT_Y, LCD_SCOPE_PLOT_WIDTH,
                LCD_SCOPE_PLOT_HEIGHT, bg);
  lcd_draw_box(LCD_SCOPE_PLOT_X, LCD_SCOPE_PLOT_Y, LCD_SCOPE_PLOT_WIDTH,
               LCD_SCOPE_PLOT_HEIGHT, grid, bg);
  lcd_draw_hline(LCD_SCOPE_PLOT_X,
                 (uint16_t)(LCD_SCOPE_PLOT_Y + (LCD_SCOPE_PLOT_HEIGHT / 2U)),
                 LCD_SCOPE_PLOT_WIDTH, grid);
  lcd_draw_vline((uint16_t)(LCD_SCOPE_PLOT_X + (LCD_SCOPE_PLOT_WIDTH / 2U)),
                 LCD_SCOPE_PLOT_Y, LCD_SCOPE_PLOT_HEIGHT, grid);
}

static void lcd_draw_xy_trace(const scope_capture_frame_t *frame) {
  uint16_t trace = lcd_rgb565(255, 166, 78);
  uint16_t warn = lcd_rgb565(255, 208, 116);
  uint16_t panel = lcd_rgb565(10, 20, 42);
  uint16_t count = APP_SCOPE_SAMPLE_COUNT;

  lcd_draw_xy_frame();

  if (frame == NULL || !frame->ch_a.valid || !frame->ch_b.valid) {
    lcd_draw_string((uint16_t)(LCD_SCOPE_PLOT_X + 92U),
                    (uint16_t)(LCD_SCOPE_PLOT_Y + 50U),
                    "CONNECT CH-A AND CH-B", warn, panel, 2);
    return;
  }

  if (frame->ch_a.sample_count < count) {
    count = frame->ch_a.sample_count;
  }
  if (frame->ch_b.sample_count < count) {
    count = frame->ch_b.sample_count;
  }

  if (count < 2U) {
    return;
  }

  for (uint16_t i = 1U; i < count; ++i) {
    lcd_draw_line(xy_map_x(frame->ch_a.samples[i - 1U]),
                  xy_map_y(frame->ch_b.samples[i - 1U]),
                  xy_map_x(frame->ch_a.samples[i]),
                  xy_map_y(frame->ch_b.samples[i]),
                  trace);
  }
}

void lcd_draw_xy_dynamic(const ui_ctrl_view_t *ui,
                         const signal_measure_result_t *measurement) {
  scope_capture_frame_t frame = {0};
  const signal_gen_dac_status_t *dac = signal_gen_dac_current();
  uint16_t panel = lcd_rgb565(7, 16, 34);
  uint16_t border = lcd_rgb565(54, 122, 178);
  uint16_t accent = lcd_rgb565(255, 182, 86);
  uint16_t text = lcd_rgb565(236, 246, 255);
  char line[40];
  uint32_t now = HAL_GetTick();

  (void)measurement;
  signal_capture_adc_read_frame(&frame, now);

  lcd_draw_card(18, 14, 198, 38, border, panel, accent);
  lcd_draw_card(222, 14, 116, 38, border, panel, accent);
  lcd_draw_string(26, 20, "XY MODE", text, panel, 1);
  lcd_draw_string(26, 30, "X=CH-A(PA0)", text, panel, 1);
  lcd_draw_string(26, 40, "Y=CH-B(PA6)", text, panel, 1);
  lcd_draw_string(230, 20, "OUTPUT", text, panel, 1);
  (void)snprintf(line, sizeof(line), "%s %luHZ",
                 waveform_short_name(dac->waveform),
                 (unsigned long)dac->frequency_hz);
  lcd_draw_string(230, 30, line, accent, panel, 1);
  lcd_draw_string(230, 40, "PA4/PA5 DAC", text, panel, 1);

  lcd_draw_xy_trace(&frame);
  lcd_draw_footer(ui);
}
