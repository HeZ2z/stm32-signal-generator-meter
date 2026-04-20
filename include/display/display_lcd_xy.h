#ifndef DISPLAY_LCD_XY_H
#define DISPLAY_LCD_XY_H

#include "signal_capture/signal_capture_adc.h"
#include "signal_gen/signal_gen_dac.h"
#include "signal_measure/signal_measure.h"
#include "ui/ui_ctrl.h"

void display_lcd_xy_init(void);
void lcd_xy_plot_refresh(const scope_capture_frame_t *frame, bool force_full);
void lcd_xy_info_refresh(const signal_gen_dac_status_t *dac);
void lcd_draw_xy_dynamic(const ui_ctrl_view_t *ui,
                         const signal_measure_result_t *measurement);

#endif
