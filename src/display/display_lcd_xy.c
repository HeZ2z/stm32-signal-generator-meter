#include "display/display_lcd_xy.h"

#include <stdio.h>
#include <string.h>

#include "display/display_lcd_scene.h"
#include "display/display_lcd_scope.h"
#include "display/display_lcd_xy_logic.h"
#include "display/lcd_prim.h"
#include "signal_capture/signal_capture_adc.h"
#include "signal_gen/signal_gen_dac.h"

typedef struct {
  bool info_valid;
  bool frame_valid;
  char output_line[40];
  lcd_xy_trace_t trace;
} lcd_xy_state_t;

static lcd_xy_state_t lcd_xy_state;

static void lcd_draw_xy_info_card(const signal_gen_dac_status_t *dac) {
  uint16_t panel = lcd_rgb565(7, 16, 34);
  uint16_t border = lcd_rgb565(54, 122, 178);
  uint16_t accent = lcd_rgb565(255, 182, 86);
  uint16_t text = lcd_rgb565(236, 246, 255);
  char line[40];

  (void)snprintf(line, sizeof(line), "%s %luHZ",
                 signal_gen_dac_waveform_short_name(dac->waveform),
                 (unsigned long)dac->frequency_hz);
  if (lcd_xy_state.info_valid &&
      strcmp(lcd_xy_state.output_line, line) == 0) {
    return;
  }

  lcd_draw_card(18, 14, 198, 38, border, panel, accent);
  lcd_draw_card(222, 14, 116, 38, border, panel, accent);
  lcd_draw_string(26, 20, "XY MODE", text, panel, 1);
  lcd_draw_string(26, 30, "X=CH-A(PA0)", text, panel, 1);
  lcd_draw_string(26, 40, "Y=CH-B(PA6)", text, panel, 1);
  lcd_draw_string(230, 20, "OUTPUT", text, panel, 1);
  lcd_draw_string(230, 30, line, accent, panel, 1);
  lcd_draw_string(230, 40, "PA4/PA5 DAC", text, panel, 1);

  lcd_xy_state.info_valid = true;
  (void)snprintf(lcd_xy_state.output_line, sizeof(lcd_xy_state.output_line),
                 "%s", line);
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

static void lcd_restore_xy_region(const lcd_xy_bounds_t *bounds) {
  uint16_t bg = lcd_rgb565(10, 20, 42);
  uint16_t grid = lcd_rgb565(32, 66, 108);
  uint16_t width;
  uint16_t height;
  uint16_t top = LCD_SCOPE_PLOT_Y;
  uint16_t bottom = (uint16_t)(LCD_SCOPE_PLOT_Y + LCD_SCOPE_PLOT_HEIGHT - 1U);
  uint16_t left = LCD_SCOPE_PLOT_X;
  uint16_t right = (uint16_t)(LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH - 1U);
  uint16_t center_x = (uint16_t)(LCD_SCOPE_PLOT_X + (LCD_SCOPE_PLOT_WIDTH / 2U));
  uint16_t center_y = (uint16_t)(LCD_SCOPE_PLOT_Y + (LCD_SCOPE_PLOT_HEIGHT / 2U));

  if (bounds == NULL || !bounds->valid) {
    return;
  }

  width = (uint16_t)(bounds->max_x - bounds->min_x + 1U);
  height = (uint16_t)(bounds->max_y - bounds->min_y + 1U);
  lcd_fill_rect(bounds->min_x, bounds->min_y, width, height, bg);

  if (bounds->min_y <= top && bounds->max_y >= top) {
    lcd_draw_hline(bounds->min_x, top, width, grid);
  }
  if (bounds->min_y <= bottom && bounds->max_y >= bottom) {
    lcd_draw_hline(bounds->min_x, bottom, width, grid);
  }
  if (bounds->min_x <= left && bounds->max_x >= left) {
    lcd_draw_vline(left, bounds->min_y, height, grid);
  }
  if (bounds->min_x <= right && bounds->max_x >= right) {
    lcd_draw_vline(right, bounds->min_y, height, grid);
  }
  if (bounds->min_y <= center_y && bounds->max_y >= center_y) {
    lcd_draw_hline(bounds->min_x, center_y, width, grid);
  }
  if (bounds->min_x <= center_x && bounds->max_x >= center_x) {
    lcd_draw_vline(center_x, bounds->min_y, height, grid);
  }
}

static void lcd_draw_xy_trace_lines(const lcd_xy_trace_t *trace) {
  uint16_t color = lcd_rgb565(255, 166, 78);

  if (trace == NULL || !trace->valid || trace->point_count < 2U) {
    return;
  }

  for (uint16_t i = 1U; i < trace->point_count; ++i) {
    lcd_draw_line(trace->x_points[i - 1U], trace->y_points[i - 1U],
                  trace->x_points[i], trace->y_points[i], color);
  }
}

static void lcd_draw_xy_no_signal(void) {
  uint16_t warn = lcd_rgb565(255, 208, 116);
  uint16_t panel = lcd_rgb565(10, 20, 42);

  lcd_draw_string((uint16_t)(LCD_SCOPE_PLOT_X + 92U),
                  (uint16_t)(LCD_SCOPE_PLOT_Y + 50U),
                  "CONNECT CH-A AND CH-B", warn, panel, 2);
}

void display_lcd_xy_init(void) {
  (void)memset(&lcd_xy_state, 0, sizeof(lcd_xy_state));
  lcd_xy_trace_reset(&lcd_xy_state.trace);
}

void lcd_xy_plot_refresh(const scope_capture_frame_t *frame, bool force_full) {
  lcd_xy_trace_t current = {0};
  lcd_xy_bounds_t dirty = {0};

  lcd_xy_trace_build(frame, &current);
  if (force_full || (lcd_xy_state.frame_valid != current.valid)) {
    lcd_draw_xy_frame();
    if (current.valid) {
      lcd_draw_xy_trace_lines(&current);
    } else {
      lcd_draw_xy_no_signal();
    }
  } else if (current.valid) {
    if (lcd_xy_union_dirty_bounds(&lcd_xy_state.trace, &current, 2U, &dirty)) {
      lcd_restore_xy_region(&dirty);
    } else {
      lcd_draw_xy_frame();
    }
    lcd_draw_xy_trace_lines(&current);
  }

  lcd_xy_state.trace = current;
  lcd_xy_state.frame_valid = current.valid;
}

void lcd_xy_info_refresh(const signal_gen_dac_status_t *dac) {
  if (dac == NULL) {
    return;
  }

  lcd_draw_xy_info_card(dac);
}

void lcd_draw_xy_dynamic(const ui_ctrl_view_t *ui,
                         const signal_measure_result_t *measurement) {
  scope_capture_frame_t frame = {0};
  const signal_gen_dac_status_t *dac = signal_gen_dac_current();
  uint32_t now = HAL_GetTick();

  (void)measurement;
  signal_capture_adc_read_frame(&frame, now);

  lcd_xy_info_refresh(dac);
  lcd_xy_plot_refresh(&frame, !lcd_xy_state.trace.valid);
  lcd_draw_footer(ui);
}
