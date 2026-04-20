#include "display/display_lcd_xy_logic.h"

#include <stddef.h>

static uint16_t xy_map_x(uint16_t raw) {
  return (uint16_t)(LCD_SCOPE_PLOT_X +
                    (((uint32_t)raw * (LCD_SCOPE_PLOT_WIDTH - 1U)) / 4095U));
}

static uint16_t xy_map_y(uint16_t raw) {
  return (uint16_t)(LCD_SCOPE_PLOT_Y + (LCD_SCOPE_PLOT_HEIGHT - 1U) -
                    (((uint32_t)raw * (LCD_SCOPE_PLOT_HEIGHT - 1U)) / 4095U));
}

void lcd_xy_trace_reset(lcd_xy_trace_t *trace) {
  if (trace == NULL) {
    return;
  }

  trace->valid = false;
  trace->point_count = 0U;
  trace->bounds.valid = false;
  trace->bounds.min_x = 0U;
  trace->bounds.max_x = 0U;
  trace->bounds.min_y = 0U;
  trace->bounds.max_y = 0U;
}

void lcd_xy_trace_build(const scope_capture_frame_t *frame, lcd_xy_trace_t *trace) {
  uint16_t count = APP_SCOPE_SAMPLE_COUNT;

  lcd_xy_trace_reset(trace);
  if (trace == NULL || frame == NULL || !frame->ch_a.valid || !frame->ch_b.valid) {
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

  trace->valid = true;
  trace->point_count = count;
  trace->bounds.valid = true;

  for (uint16_t i = 0U; i < count; ++i) {
    uint16_t x = xy_map_x(frame->ch_a.samples[i]);
    uint16_t y = xy_map_y(frame->ch_b.samples[i]);

    trace->x_points[i] = x;
    trace->y_points[i] = y;

    if (i == 0U) {
      trace->bounds.min_x = x;
      trace->bounds.max_x = x;
      trace->bounds.min_y = y;
      trace->bounds.max_y = y;
      continue;
    }

    if (x < trace->bounds.min_x) {
      trace->bounds.min_x = x;
    }
    if (x > trace->bounds.max_x) {
      trace->bounds.max_x = x;
    }
    if (y < trace->bounds.min_y) {
      trace->bounds.min_y = y;
    }
    if (y > trace->bounds.max_y) {
      trace->bounds.max_y = y;
    }
  }
}

bool lcd_xy_union_dirty_bounds(const lcd_xy_trace_t *previous,
                               const lcd_xy_trace_t *current,
                               uint16_t margin,
                               lcd_xy_bounds_t *bounds) {
  uint16_t min_x = 0U;
  uint16_t max_x = 0U;
  uint16_t min_y = 0U;
  uint16_t max_y = 0U;
  bool have_previous = previous != NULL && previous->valid && previous->bounds.valid;
  bool have_current = current != NULL && current->valid && current->bounds.valid;

  if (bounds == NULL) {
    return false;
  }

  bounds->valid = false;
  if (!have_previous && !have_current) {
    return false;
  }

  if (have_previous) {
    min_x = previous->bounds.min_x;
    max_x = previous->bounds.max_x;
    min_y = previous->bounds.min_y;
    max_y = previous->bounds.max_y;
  }

  if (have_current) {
    if (!have_previous || current->bounds.min_x < min_x) {
      min_x = current->bounds.min_x;
    }
    if (!have_previous || current->bounds.max_x > max_x) {
      max_x = current->bounds.max_x;
    }
    if (!have_previous || current->bounds.min_y < min_y) {
      min_y = current->bounds.min_y;
    }
    if (!have_previous || current->bounds.max_y > max_y) {
      max_y = current->bounds.max_y;
    }
  }

  if (min_x > (uint16_t)(LCD_SCOPE_PLOT_X + margin)) {
    min_x = (uint16_t)(min_x - margin);
  } else {
    min_x = LCD_SCOPE_PLOT_X;
  }
  if (min_y > (uint16_t)(LCD_SCOPE_PLOT_Y + margin)) {
    min_y = (uint16_t)(min_y - margin);
  } else {
    min_y = LCD_SCOPE_PLOT_Y;
  }

  max_x = (uint16_t)(max_x + margin);
  max_y = (uint16_t)(max_y + margin);
  if (max_x >= (LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH)) {
    max_x = (uint16_t)(LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH - 1U);
  }
  if (max_y >= (LCD_SCOPE_PLOT_Y + LCD_SCOPE_PLOT_HEIGHT)) {
    max_y = (uint16_t)(LCD_SCOPE_PLOT_Y + LCD_SCOPE_PLOT_HEIGHT - 1U);
  }

  bounds->valid = true;
  bounds->min_x = min_x;
  bounds->max_x = max_x;
  bounds->min_y = min_y;
  bounds->max_y = max_y;
  return true;
}
