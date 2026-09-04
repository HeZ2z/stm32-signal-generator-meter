#include "display/lcd_prim.h"

#include <stdio.h>
#include <string.h>

#include "board/board_config.h"
#include "display/lcd_font.h"
#include "stm32f4xx_ll_dma2d.h"
#include "ui/ui_ctrl.h"

/* Apollo F429 将 LTDC 单层帧缓冲直接映射到外部 SDRAM 首地址。 */
static uint16_t *const framebuffer = (uint16_t *)APP_LCD_FRAMEBUFFER_ADDRESS;
static bool lcd_dma2d_available;

#define LCD_DMA2D_MIN_AREA 128U

uint16_t lcd_rgb565(uint8_t red, uint8_t green, uint8_t blue) {
  return (uint16_t)(((red & 0xF8U) << 8) | ((green & 0xFCU) << 3) | (blue >> 3));
}

static void lcd_fill_rect_cpu(uint16_t x,
                              uint16_t y,
                              uint16_t width,
                              uint16_t height,
                              uint16_t color) {
  for (uint16_t row = 0; row < height; ++row) {
    if ((y + row) >= APP_LCD_HEIGHT) {
      break;
    }
    for (uint16_t col = 0; col < width; ++col) {
      if ((x + col) >= APP_LCD_WIDTH) {
        break;
      }
      framebuffer[((y + row) * APP_LCD_WIDTH) + (x + col)] = color;
    }
  }
}

static bool lcd_fill_rect_dma2d(uint16_t x,
                                uint16_t y,
                                uint16_t width,
                                uint16_t height,
                                uint16_t color) {
  uint32_t start_ms = HAL_GetTick();

  if (!lcd_dma2d_available || ((uint32_t)width * (uint32_t)height) < LCD_DMA2D_MIN_AREA) {
    return false;
  }

  LL_DMA2D_Abort(DMA2D);
  LL_DMA2D_SetMode(DMA2D, LL_DMA2D_MODE_R2M);
  LL_DMA2D_SetOutputColorMode(DMA2D, LL_DMA2D_OUTPUT_MODE_RGB565);
  LL_DMA2D_SetOutputColor(DMA2D, (uint32_t)color);
  LL_DMA2D_SetOutputMemAddr(
      DMA2D, (uint32_t)&framebuffer[((uint32_t)y * APP_LCD_WIDTH) + x]);
  LL_DMA2D_SetLineOffset(DMA2D, APP_LCD_WIDTH - width);
  MODIFY_REG(DMA2D->NLR,
             DMA2D_NLR_NL | DMA2D_NLR_PL,
             ((uint32_t)height << DMA2D_NLR_NL_Pos) |
                 ((uint32_t)width << DMA2D_NLR_PL_Pos));
  LL_DMA2D_Start(DMA2D);
  while (LL_DMA2D_IsTransferOngoing(DMA2D) != 0U) {
    if ((HAL_GetTick() - start_ms) > 20U) {
      LL_DMA2D_Abort(DMA2D);
      lcd_dma2d_available = false;
      return false;
    }
  }
  return true;
}

void lcd_prim_backend_init(void) {
#if APP_LCD_USE_DMA2D
  __HAL_RCC_DMA2D_CLK_ENABLE();
  __HAL_RCC_DMA2D_FORCE_RESET();
  __HAL_RCC_DMA2D_RELEASE_RESET();
  lcd_dma2d_available = true;
#else
  lcd_dma2d_available = false;
#endif
}

bool lcd_prim_dma2d_ready(void) {
  return lcd_dma2d_available;
}

void lcd_plot(uint16_t x, uint16_t y, uint16_t color) {
  if (x >= APP_LCD_WIDTH || y >= APP_LCD_HEIGHT) {
    return;
  }
  framebuffer[(y * APP_LCD_WIDTH) + x] = color;
}

void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
  if (x >= APP_LCD_WIDTH || y >= APP_LCD_HEIGHT || width == 0U || height == 0U) {
    return;
  }
  if ((uint32_t)x + width > APP_LCD_WIDTH) {
    width = (uint16_t)(APP_LCD_WIDTH - x);
  }
  if ((uint32_t)y + height > APP_LCD_HEIGHT) {
    height = (uint16_t)(APP_LCD_HEIGHT - y);
  }

  if (!lcd_fill_rect_dma2d(x, y, width, height, color)) {
    lcd_fill_rect_cpu(x, y, width, height, color);
  }
}

void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t width, uint16_t color) {
  lcd_fill_rect(x, y, width, 1U, color);
}

void lcd_draw_vline(uint16_t x, uint16_t y, uint16_t height, uint16_t color) {
  lcd_fill_rect(x, y, 1U, height, color);
}

void lcd_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
  int32_t x = (int32_t)x0;
  int32_t y = (int32_t)y0;
  int32_t dx = (int32_t)x1 - x;
  int32_t dy = (int32_t)y1 - y;
  int32_t dx_abs;
  int32_t dy_abs;
  int32_t sx = x < (int32_t)x1 ? 1 : -1;
  int32_t sy = y < (int32_t)y1 ? 1 : -1;
  int32_t err;
  uint32_t steps = 0U;
  uint32_t max_steps = APP_LCD_WIDTH + APP_LCD_HEIGHT + 4U;

  if (dx < 0) {
    dx = -dx;
  }
  if (dy < 0) {
    dy = -dy;
  }

  dx_abs = dx;
  dy_abs = dy;
  err = dx_abs - dy_abs;

  while (steps <= max_steps) {
    lcd_plot((uint16_t)x, (uint16_t)y, color);
    if (x == (int32_t)x1 && y == (int32_t)y1) {
      break;
    }

    {
      int32_t e2 = err * 2;

      if (e2 > -dy_abs) {
        err -= dy_abs;
        x += sx;
      }
      if (e2 < dx_abs) {
        err += dx_abs;
        y += sy;
      }
    }

    ++steps;
  }
}

void lcd_draw_box(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t border, uint16_t fill) {
  lcd_fill_rect(x, y, width, height, border);
  if (width > 2U && height > 2U) {
    lcd_fill_rect((uint16_t)(x + 1U), (uint16_t)(y + 1U), (uint16_t)(width - 2U), (uint16_t)(height - 2U), fill);
  }
}

void lcd_draw_card(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                    uint16_t border, uint16_t fill, uint16_t accent) {
  lcd_fill_rect(x, y, width, height, border);
  lcd_fill_rect((uint16_t)(x + 2U), (uint16_t)(y + 2U), (uint16_t)(width - 4U), (uint16_t)(height - 4U), fill);
  lcd_fill_rect((uint16_t)(x + 2U), (uint16_t)(y + 2U), (uint16_t)(width - 4U), 4U, accent);
}

void lcd_clear(uint16_t color) {
  lcd_fill_rect(0, 0, APP_LCD_WIDTH, APP_LCD_HEIGHT, color);
}

/* ------------------------------------------------------------------ */
/*  按钮定义与查找                                                    */
/* ------------------------------------------------------------------ */

static const char *const lcd_button_labels[] = {
    [UI_HIGHLIGHT_FREQ_DOWN] = "F-1K",
    [UI_HIGHLIGHT_FREQ_UP]   = "F+1K",
    [UI_HIGHLIGHT_WAVE_TOGGLE] = "WAVE",
    [UI_HIGHLIGHT_SCREEN_TOGGLE] = "YT/XY",
    [UI_HIGHLIGHT_RESET]     = "RESET",
    [UI_HIGHLIGHT_HELP]      = "MORE",
};

extern const struct _lcd_btn_def lcd_buttons[];
const struct _lcd_btn_def lcd_buttons[] = {
    {UI_HIGHLIGHT_FREQ_DOWN,  22U, 224U,  72U, 30U},
    {UI_HIGHLIGHT_FREQ_UP,    98U, 224U,  72U, 30U},
    {UI_HIGHLIGHT_WAVE_TOGGLE, 174U, 224U, 72U, 30U},
    {UI_HIGHLIGHT_SCREEN_TOGGLE, 250U, 224U, 72U, 30U},
    {UI_HIGHLIGHT_RESET,     356U, 224U,  98U, 30U},
    {UI_HIGHLIGHT_HELP,      360U,  14U,  98U, 28U},
};

const char *lcd_button_label(int id) {
  if (id >= UI_HIGHLIGHT_NONE && id <= UI_HIGHLIGHT_HELP) {
    return lcd_button_labels[id];
  }
  return NULL;
}

static const struct _lcd_btn_def *lcd_find_button(ui_highlight_t id) {
  for (size_t i = 0; i < (sizeof(lcd_buttons) / sizeof(lcd_buttons[0])); ++i) {
    if (lcd_buttons[i].id == id) {
      return &lcd_buttons[i];
    }
  }
  return NULL;
}

void lcd_draw_button(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                     const char *label, bool highlighted) {
  uint16_t border = highlighted ? lcd_rgb565(255, 216, 118) : lcd_rgb565(100, 208, 255);
  uint16_t fill   = highlighted ? lcd_rgb565(74, 48, 8)     : lcd_rgb565(17, 42, 82);
  uint16_t text   = lcd_rgb565(234, 246, 255);
  uint16_t glow   = highlighted ? lcd_rgb565(255, 176, 72)  : lcd_rgb565(48, 92, 148);
  uint16_t text_x = (uint16_t)(x + (width >= 84U ? 12U : 8U));
  uint16_t text_y = (uint16_t)(y + (height >= 40U ? 12U : 9U));
  uint8_t scale   = width >= 84U ? 2U : 1U;

  lcd_fill_rect((uint16_t)(x - 2U), (uint16_t)(y - 2U),
                 (uint16_t)(width + 4U), (uint16_t)(height + 4U), glow);
  lcd_draw_box(x, y, width, height, border, fill);

  uint16_t ax = text_x;
  uint16_t ay = text_y;
  const char *p = label;
  while (*p != '\0') {
    lcd_draw_char(ax, ay, *p, text, fill, scale);
    ax = (uint16_t)(ax + ((5U + 1U) * scale));
    ++p;
  }
}

void lcd_redraw_button(int id, bool highlighted) {
  const struct _lcd_btn_def *btn = lcd_find_button((ui_highlight_t)id);

  if (btn == NULL) {
    return;
  }

  lcd_fill_rect((uint16_t)(btn->x - 2U),
                (uint16_t)(btn->y - 2U),
                (uint16_t)(btn->width + 4U),
                (uint16_t)(btn->height + 4U),
                lcd_rgb565(7, 16, 34));

  lcd_draw_button(btn->x, btn->y, btn->width, btn->height,
                  lcd_button_label((int)btn->id), highlighted);
}
