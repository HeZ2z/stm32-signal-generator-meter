#include "display/display_backend.h"

#include <string.h>

#include "board/board_config.h"
#include "display/display_lcd_hw.h"
#include "display/display_lcd_scene.h"
#include "display/display_lcd_scope.h"
#include "display/display_lcd_xy.h"
#include "display/lcd_prim.h"
#include "signal_measure/signal_measure.h"
#include "signal_gen/signal_gen_dac.h"
#include "ui/ui_ctrl.h"

void display_lcd_init(void) {
  display_lcd_scope_init();
  display_lcd_xy_init();
  display_lcd_hw_init();
  display_lcd_scene_init();
}

bool display_lcd_ready_impl(void) {
  return lcd_hw_ready;
}

const char *display_lcd_name_impl(void) {
  return APP_LCD_NAME;
}

const char *display_lcd_status_impl(void) {
  return display_lcd_hw_state();
}

void display_lcd_irq_handler(void) {
  display_lcd_hw_irq_handler();
}

void display_lcd_write(const char *text) {
  (void)text;
}

void display_lcd_boot_banner(void) {
  const ui_ctrl_view_t *ui = ui_ctrl_view();

  if (!lcd_hw_ready || ui == NULL) {
    return;
  }

  display_lcd_scene_set(LCD_SCENE_SPLASH);
  display_lcd_status();
}

void display_lcd_help(void) {
}

void display_lcd_status(void) {
  const ui_ctrl_view_t *ui = ui_ctrl_view();
  lcd_scene_t target_scene = LCD_SCENE_CONTROL;

  if (!lcd_hw_ready || ui == NULL) {
    return;
  }

  if (lcd_scene == LCD_SCENE_SPLASH &&
      lcd_scene_started_ms != 0U &&
      (HAL_GetTick() - lcd_scene_started_ms) >= LCD_SPLASH_DURATION_MS) {
    display_lcd_scene_set(LCD_SCENE_CONTROL);
  }

  if (lcd_scene != LCD_SCENE_SPLASH) {
    target_scene = ui->screen == UI_SCREEN_XY ? LCD_SCENE_XY : LCD_SCENE_CONTROL;
    if (lcd_scene != target_scene) {
      display_lcd_scene_set(target_scene);
    }
  }

  lcd_render_ui(ui, signal_measure_latest());
}
