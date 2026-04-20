#ifndef UI_TOUCH_MAP_H
#define UI_TOUCH_MAP_H

#include <stdbool.h>
#include <stdint.h>

#include "touch/touch.h"
#include "ui/ui_ctrl.h"

typedef struct {
  uint16_t x0;
  uint16_t y0;
  uint16_t x1;
  uint16_t y1;
} rect_t;

typedef enum {
  ACTIVE_NONE = 0,
  ACTIVE_FREQ_M1000,
  ACTIVE_FREQ_P1000,
  ACTIVE_WAVE_TOGGLE,
  ACTIVE_SCREEN_TOGGLE,
  ACTIVE_RESET,
  ACTIVE_HELP,
} active_control_t;

/* 判断触摸点是否在给定矩形内。 */
bool point_in_rect(const touch_point_t *point, rect_t rect);

/* 根据触摸点坐标返回命中控件 ID。 */
active_control_t hit_control(const touch_point_t *point, bool more_open);

/* 将 active_control 映射为 ui_highlight（用于按钮高亮）。 */
ui_highlight_t active_highlight_map(active_control_t control);

#endif
