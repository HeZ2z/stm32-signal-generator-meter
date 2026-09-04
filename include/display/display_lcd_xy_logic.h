#ifndef DISPLAY_LCD_XY_LOGIC_H
#define DISPLAY_LCD_XY_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#include "display/lcd_prim.h"
#include "signal_capture/signal_capture_adc.h"

typedef struct {
  bool valid;
  uint16_t min_x;
  uint16_t max_x;
  uint16_t min_y;
  uint16_t max_y;
} lcd_xy_bounds_t;

typedef struct {
  bool valid;
  uint16_t point_count;
  uint16_t x_points[APP_SCOPE_SAMPLE_COUNT];
  uint16_t y_points[APP_SCOPE_SAMPLE_COUNT];
  lcd_xy_bounds_t bounds;
} lcd_xy_trace_t;

void lcd_xy_trace_reset(lcd_xy_trace_t *trace);
void lcd_xy_trace_build(const scope_capture_frame_t *frame, lcd_xy_trace_t *trace);
bool lcd_xy_union_dirty_bounds(const lcd_xy_trace_t *previous,
                               const lcd_xy_trace_t *current,
                               uint16_t margin,
                               lcd_xy_bounds_t *bounds);

#endif
