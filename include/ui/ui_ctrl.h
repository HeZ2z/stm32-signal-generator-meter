#ifndef UI_CTRL_H
#define UI_CTRL_H

#include <stdbool.h>

#include "signal_gen/signal_gen_dac.h"
#include "touch/touch.h"

typedef enum {
  UI_SCREEN_CONTROL = 0,
  UI_SCREEN_XY,
  UI_SCREEN_CALIBRATION,
} ui_screen_t;

typedef enum {
  UI_HIGHLIGHT_NONE = 0,
  UI_HIGHLIGHT_FREQ_DOWN,
  UI_HIGHLIGHT_FREQ_UP,
  UI_HIGHLIGHT_WAVE_TOGGLE,
  UI_HIGHLIGHT_SCREEN_TOGGLE,
  UI_HIGHLIGHT_RESET,
  UI_HIGHLIGHT_HELP,
} ui_highlight_t;

typedef struct {
  ui_screen_t screen;
  bool touch_ready;
  bool touch_pressed;
  bool more_open;
  ui_highlight_t highlight;
  signal_gen_dac_config_t active_config;
  signal_gen_dac_config_t pending_config;
  touch_point_t last_touch;
  touch_point_t calibration_target;
  char title[32];
  char footer[64];
} ui_ctrl_view_t;

/* 初始化串口控制状态。 */
void ui_ctrl_init(void);
/* 轮询串口输入并执行用户命令。 */
void ui_ctrl_poll(void);
/* 返回当前 LCD 触控界面状态。 */
const ui_ctrl_view_t *ui_ctrl_view(void);

#endif
