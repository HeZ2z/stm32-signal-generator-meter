#ifndef DISPLAY_LCD_SCENE_H
#define DISPLAY_LCD_SCENE_H

#include <stdbool.h>
#include <stdint.h>

#include "signal_gen/signal_gen.h"
#include "signal_measure/signal_measure.h"
#include "ui/ui_ctrl.h"

typedef enum {
  LCD_SCENE_SPLASH = 0,
  LCD_SCENE_CONTROL,
} lcd_scene_t;

extern lcd_scene_t lcd_scene;
extern uint32_t lcd_scene_started_ms;

void display_lcd_scene_init(void);
void display_lcd_scene_set(lcd_scene_t scene);

void lcd_draw_splash(void);
void lcd_draw_control_static(const ui_ctrl_view_t *ui);
void lcd_draw_footer(const ui_ctrl_view_t *ui);
void lcd_draw_more_overlay(void);
void lcd_restore_more_overlay(const ui_ctrl_view_t *ui,
                              const signal_measure_result_t *measurement);
void lcd_render_ui(const ui_ctrl_view_t *ui,
                    const signal_gen_config_t *config,
                    const signal_measure_result_t *measurement);

#endif
