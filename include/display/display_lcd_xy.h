#ifndef DISPLAY_LCD_XY_H
#define DISPLAY_LCD_XY_H

#include "signal_measure/signal_measure.h"
#include "ui/ui_ctrl.h"

void lcd_draw_xy_dynamic(const ui_ctrl_view_t *ui,
                         const signal_measure_result_t *measurement);

#endif
