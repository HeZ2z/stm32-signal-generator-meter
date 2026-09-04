#include "ui/ui_touch_map.h"

bool point_in_rect(const touch_point_t *point, rect_t rect) {
  return point->x >= rect.x0 && point->x <= rect.x1 &&
         point->y >= rect.y0 && point->y <= rect.y1;
}

ui_highlight_t active_highlight_map(active_control_t control) {
  switch (control) {
    case ACTIVE_FREQ_M1000:
      return UI_HIGHLIGHT_FREQ_DOWN;
    case ACTIVE_FREQ_P1000:
      return UI_HIGHLIGHT_FREQ_UP;
    case ACTIVE_WAVE_TOGGLE:
      return UI_HIGHLIGHT_WAVE_TOGGLE;
    case ACTIVE_SCREEN_TOGGLE:
      return UI_HIGHLIGHT_SCREEN_TOGGLE;
    case ACTIVE_RESET:
      return UI_HIGHLIGHT_RESET;
    case ACTIVE_HELP:
      return UI_HIGHLIGHT_HELP;
    case ACTIVE_NONE:
    default:
      return UI_HIGHLIGHT_NONE;
  }
}

active_control_t hit_control(const touch_point_t *point, bool more_open) {
  if (more_open) {
    if (point_in_rect(point, (rect_t){360U, 14U, 458U, 42U})) {
      return ACTIVE_HELP;
    }
    return ACTIVE_NONE;
  }

  if (point_in_rect(point, (rect_t){22U, 224U, 94U, 254U})) {
    return ACTIVE_FREQ_M1000;
  }
  if (point_in_rect(point, (rect_t){98U, 224U, 170U, 254U})) {
    return ACTIVE_FREQ_P1000;
  }
  if (point_in_rect(point, (rect_t){174U, 224U, 246U, 254U})) {
    return ACTIVE_WAVE_TOGGLE;
  }
  if (point_in_rect(point, (rect_t){250U, 224U, 322U, 254U})) {
    return ACTIVE_SCREEN_TOGGLE;
  }
  if (point_in_rect(point, (rect_t){356U, 224U, 454U, 254U})) {
    return ACTIVE_RESET;
  }
  if (point_in_rect(point, (rect_t){360U, 14U, 458U, 42U})) {
    return ACTIVE_HELP;
  }
  return ACTIVE_NONE;
}
