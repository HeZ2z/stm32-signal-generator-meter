#ifndef LCD_FONT_H
#define LCD_FONT_H

#include <stdint.h>

/* 绘制单个 5x7 字符。 */
void lcd_draw_char(uint16_t x, uint16_t y, char ch,
                   uint16_t fg, uint16_t bg, uint8_t scale);

#endif
