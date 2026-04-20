#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "display/display_lcd_xy_logic.h"

static int failures = 0;

static void expect(bool condition, const char *message) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s\n", message);
    failures++;
  }
}

static scope_capture_frame_t make_frame(uint16_t a0,
                                        uint16_t a1,
                                        uint16_t b0,
                                        uint16_t b1) {
  scope_capture_frame_t frame = {0};

  frame.ch_a.valid = true;
  frame.ch_b.valid = true;
  frame.ch_a.sample_count = 2U;
  frame.ch_b.sample_count = 2U;
  frame.ch_a.samples[0] = a0;
  frame.ch_a.samples[1] = a1;
  frame.ch_b.samples[0] = b0;
  frame.ch_b.samples[1] = b1;
  return frame;
}

static void test_build_valid_trace(void) {
  lcd_xy_trace_t trace = {0};
  scope_capture_frame_t frame = make_frame(0U, 4095U, 0U, 4095U);

  lcd_xy_trace_build(&frame, &trace);

  expect(trace.valid, "trace should be valid");
  expect(trace.point_count == 2U, "point count");
  expect(trace.bounds.valid, "bounds valid");
  expect(trace.bounds.min_x == LCD_SCOPE_PLOT_X, "min x maps to plot start");
  expect(trace.bounds.max_x == (uint16_t)(LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH - 1U),
         "max x maps to plot end");
}

static void test_reject_invalid_frame(void) {
  lcd_xy_trace_t trace = {0};
  scope_capture_frame_t frame = {0};

  lcd_xy_trace_build(&frame, &trace);

  expect(!trace.valid, "invalid frame rejected");
  expect(trace.point_count == 0U, "invalid frame point count");
}

static void test_union_dirty_bounds_expands_and_clips(void) {
  lcd_xy_trace_t previous = {0};
  lcd_xy_trace_t current = {0};
  lcd_xy_bounds_t dirty = {0};

  previous.valid = true;
  previous.bounds.valid = true;
  previous.bounds.min_x = LCD_SCOPE_PLOT_X;
  previous.bounds.max_x = (uint16_t)(LCD_SCOPE_PLOT_X + 10U);
  previous.bounds.min_y = LCD_SCOPE_PLOT_Y;
  previous.bounds.max_y = (uint16_t)(LCD_SCOPE_PLOT_Y + 10U);

  current.valid = true;
  current.bounds.valid = true;
  current.bounds.min_x = (uint16_t)(LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH - 6U);
  current.bounds.max_x = (uint16_t)(LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH - 1U);
  current.bounds.min_y = (uint16_t)(LCD_SCOPE_PLOT_Y + LCD_SCOPE_PLOT_HEIGHT - 6U);
  current.bounds.max_y = (uint16_t)(LCD_SCOPE_PLOT_Y + LCD_SCOPE_PLOT_HEIGHT - 1U);

  expect(lcd_xy_union_dirty_bounds(&previous, &current, 2U, &dirty),
         "dirty union should exist");
  expect(dirty.min_x == LCD_SCOPE_PLOT_X, "dirty min x clipped");
  expect(dirty.min_y == LCD_SCOPE_PLOT_Y, "dirty min y clipped");
  expect(dirty.max_x == (uint16_t)(LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH - 1U),
         "dirty max x clipped");
  expect(dirty.max_y == (uint16_t)(LCD_SCOPE_PLOT_Y + LCD_SCOPE_PLOT_HEIGHT - 1U),
         "dirty max y clipped");
}

static void test_union_dirty_bounds_only_current(void) {
  lcd_xy_bounds_t dirty = {0};

  lcd_xy_trace_t current = {0};
  current.valid = true;
  current.bounds.valid = true;
  current.bounds.min_x = (uint16_t)(LCD_SCOPE_PLOT_X + 30U);
  current.bounds.max_x = (uint16_t)(LCD_SCOPE_PLOT_X + 100U);
  current.bounds.min_y = (uint16_t)(LCD_SCOPE_PLOT_Y + 20U);
  current.bounds.max_y = (uint16_t)(LCD_SCOPE_PLOT_Y + 80U);

  expect(lcd_xy_union_dirty_bounds(NULL, &current, 2U, &dirty),
         "union with NULL previous should succeed");
  expect(dirty.valid, "dirty valid");
  expect(dirty.min_x == (uint16_t)(LCD_SCOPE_PLOT_X + 28U), "min_x expanded by margin");
  expect(dirty.max_x == (uint16_t)(LCD_SCOPE_PLOT_X + 102U), "max_x expanded by margin");
  expect(dirty.min_y == (uint16_t)(LCD_SCOPE_PLOT_Y + 18U), "min_y expanded by margin");
  expect(dirty.max_y == (uint16_t)(LCD_SCOPE_PLOT_Y + 82U), "max_y expanded by margin");
}

static void test_union_dirty_bounds_only_previous(void) {
  lcd_xy_bounds_t dirty = {0};

  lcd_xy_trace_t previous = {0};
  previous.valid = true;
  previous.bounds.valid = true;
  previous.bounds.min_x = (uint16_t)(LCD_SCOPE_PLOT_X + 30U);
  previous.bounds.max_x = (uint16_t)(LCD_SCOPE_PLOT_X + 100U);
  previous.bounds.min_y = (uint16_t)(LCD_SCOPE_PLOT_Y + 20U);
  previous.bounds.max_y = (uint16_t)(LCD_SCOPE_PLOT_Y + 80U);

  expect(lcd_xy_union_dirty_bounds(&previous, NULL, 2U, &dirty),
         "union with NULL current should succeed");
  expect(dirty.valid, "dirty valid");
  expect(dirty.min_x == (uint16_t)(LCD_SCOPE_PLOT_X + 28U), "min_x expanded by margin");
  expect(dirty.max_x == (uint16_t)(LCD_SCOPE_PLOT_X + 102U), "max_x expanded by margin");
  expect(dirty.min_y == (uint16_t)(LCD_SCOPE_PLOT_Y + 18U), "min_y expanded by margin");
  expect(dirty.max_y == (uint16_t)(LCD_SCOPE_PLOT_Y + 82U), "max_y expanded by margin");
}

static void test_union_dirty_bounds_previous_invalid_current_valid(void) {
  lcd_xy_bounds_t dirty = {0};

  lcd_xy_trace_t previous = {0};
  previous.valid = false;

  lcd_xy_trace_t current = {0};
  current.valid = true;
  current.bounds.valid = true;
  current.bounds.min_x = (uint16_t)(LCD_SCOPE_PLOT_X + 50U);
  current.bounds.max_x = (uint16_t)(LCD_SCOPE_PLOT_X + 150U);
  current.bounds.min_y = (uint16_t)(LCD_SCOPE_PLOT_Y + 30U);
  current.bounds.max_y = (uint16_t)(LCD_SCOPE_PLOT_Y + 90U);

  expect(lcd_xy_union_dirty_bounds(&previous, &current, 2U, &dirty),
         "invalid previous should be treated as no previous");
  expect(dirty.valid, "dirty valid");
  expect(dirty.min_x == (uint16_t)(LCD_SCOPE_PLOT_X + 48U), "min_x from current expanded");
  expect(dirty.max_x == (uint16_t)(LCD_SCOPE_PLOT_X + 152U), "max_x from current expanded");
}

static void test_union_dirty_bounds_margin_zero(void) {
  lcd_xy_bounds_t dirty = {0};

  lcd_xy_trace_t previous = {0};
  previous.valid = true;
  previous.bounds.valid = true;
  previous.bounds.min_x = (uint16_t)(LCD_SCOPE_PLOT_X + 50U);
  previous.bounds.max_x = (uint16_t)(LCD_SCOPE_PLOT_X + 150U);
  previous.bounds.min_y = (uint16_t)(LCD_SCOPE_PLOT_Y + 30U);
  previous.bounds.max_y = (uint16_t)(LCD_SCOPE_PLOT_Y + 90U);

  lcd_xy_trace_t current = {0};
  current.valid = true;
  current.bounds.valid = true;
  current.bounds.min_x = (uint16_t)(LCD_SCOPE_PLOT_X + 60U);
  current.bounds.max_x = (uint16_t)(LCD_SCOPE_PLOT_X + 140U);
  current.bounds.min_y = (uint16_t)(LCD_SCOPE_PLOT_Y + 40U);
  current.bounds.max_y = (uint16_t)(LCD_SCOPE_PLOT_Y + 80U);

  expect(lcd_xy_union_dirty_bounds(&previous, &current, 0U, &dirty),
         "margin zero should produce union of bounds without expansion");
  expect(dirty.valid, "dirty valid");
  expect(dirty.min_x == (uint16_t)(LCD_SCOPE_PLOT_X + 50U), "min_x from previous");
  expect(dirty.max_x == (uint16_t)(LCD_SCOPE_PLOT_X + 150U), "max_x from previous");
  expect(dirty.min_y == (uint16_t)(LCD_SCOPE_PLOT_Y + 30U), "min_y from previous");
  expect(dirty.max_y == (uint16_t)(LCD_SCOPE_PLOT_Y + 90U), "max_y from previous");
}

static void test_union_dirty_bounds_rejects_null_bounds(void) {
  expect(!lcd_xy_union_dirty_bounds(NULL, NULL, 2U, NULL),
         "NULL bounds output should be rejected");
  expect(!lcd_xy_union_dirty_bounds(NULL, NULL, 0U, NULL),
         "NULL bounds always false");
}

int main(void) {
  test_build_valid_trace();
  test_reject_invalid_frame();
  test_union_dirty_bounds_expands_and_clips();
  test_union_dirty_bounds_only_current();
  test_union_dirty_bounds_only_previous();
  test_union_dirty_bounds_previous_invalid_current_valid();
  test_union_dirty_bounds_margin_zero();
  test_union_dirty_bounds_rejects_null_bounds();

  if (failures != 0) {
    return 1;
  }

  puts("PASS: test_display_lcd_xy_logic");
  return 0;
}
