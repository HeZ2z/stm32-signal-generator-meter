#ifndef LCD_PRIM_H
#define LCD_PRIM_H

#include <stdbool.h>
#include <stdint.h>

/* ui_highlight_t 由 ui/ui_ctrl.h 提供。 */
#include "ui/ui_ctrl.h"

/* LCD 示波器绘制区域的屏幕坐标宏（像素单位）。 */
#define LCD_SCOPE_X 18U
#define LCD_SCOPE_Y 64U
#define LCD_SCOPE_WIDTH 444U
#define LCD_SCOPE_HEIGHT 142U
#define LCD_SCOPE_PLOT_X 28U
#define LCD_SCOPE_PLOT_Y 74U
#define LCD_SCOPE_PLOT_WIDTH 424U
#define LCD_SCOPE_PLOT_HEIGHT 122U
#define LCD_MORE_X 72U
#define LCD_MORE_Y 56U
#define LCD_MORE_WIDTH 336U
#define LCD_MORE_HEIGHT 144U

/* 基础绘图原语。 */
void lcd_prim_backend_init(void);
bool lcd_prim_dma2d_ready(void);
uint16_t lcd_rgb565(uint8_t red, uint8_t green, uint8_t blue);
void lcd_plot(uint16_t x, uint16_t y, uint16_t color);
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t width, uint16_t color);
void lcd_draw_vline(uint16_t x, uint16_t y, uint16_t height, uint16_t color);
void lcd_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void lcd_draw_box(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t border, uint16_t fill);
void lcd_draw_card(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                    uint16_t border, uint16_t fill, uint16_t accent);
void lcd_clear(uint16_t color);

/* 按钮绘制（参数 id 传 int，由调用方保证与 ui_highlight_t 枚举值对应）。 */
void lcd_draw_button(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                     const char *label, bool highlighted);
void lcd_redraw_button(int id, bool highlighted);
const char *lcd_button_label(int id);

/* 按钮定义（使用命名 struct 类型，确保 extern 与定义类型一致）。 */
struct _lcd_btn_def {
  ui_highlight_t id;
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
};
extern const struct _lcd_btn_def lcd_buttons[];

#endif
