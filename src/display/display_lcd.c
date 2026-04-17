#include "display/display_backend.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "board/board_config.h"
#include "scope/scope_render_logic.h"
#include "signal_capture/signal_capture_adc.h"
#include "touch/touch.h"
#include "ui/ui_ctrl.h"

#define SDRAM_MODEREG_BURST_LENGTH_1          0x0000U
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   0x0000U
#define SDRAM_MODEREG_CAS_LATENCY_3           0x0030U
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD 0x0000U
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE  0x0200U

/* LCD 状态页在屏幕上显示的四行文本。 */
typedef struct {
  char header[40];
  char set_line[40];
  char meas_line[40];
  char err_line[40];
  char touch_line[40];
  char hint_line[40];
} lcd_status_view_t;

typedef enum {
  LCD_SCENE_SPLASH = 0,
  LCD_SCENE_CONTROL,
} lcd_scene_t;

static LTDC_HandleTypeDef lcd_handle;
static SDRAM_HandleTypeDef sdram_handle;
static bool lcd_ready;
static char lcd_state[64] = APP_LCD_STATUS;
static lcd_scene_t lcd_scene = LCD_SCENE_SPLASH;
static uint32_t lcd_scene_started_ms;
static uint32_t lcd_last_dynamic_refresh_ms;

#define LCD_SPLASH_DURATION_MS 1800U
#define LCD_DYNAMIC_REFRESH_MS APP_SCOPE_LCD_REFRESH_MS
#define LCD_MORE_X 72U
#define LCD_MORE_Y 56U
#define LCD_MORE_WIDTH 336U
#define LCD_MORE_HEIGHT 144U
#define LCD_SCOPE_X 18U
#define LCD_SCOPE_Y 64U
#define LCD_SCOPE_WIDTH 444U
#define LCD_SCOPE_HEIGHT 142U
#define LCD_SCOPE_PLOT_X 28U
#define LCD_SCOPE_PLOT_Y 74U
#define LCD_SCOPE_PLOT_WIDTH 424U
#define LCD_SCOPE_PLOT_HEIGHT 122U

typedef struct {
  bool valid;
  lcd_scene_t scene;
  bool more_open;
  ui_highlight_t highlight;
  signal_gen_config_t pending_config;
  char title[32];
  char footer[64];
  char set_line[40];
  char meas_line[40];
  char err_line[40];
  char touch_line[40];
  char hint_line[40];
} lcd_ui_snapshot_t;

static lcd_ui_snapshot_t lcd_ui_last;
static scope_render_trace_t lcd_scope_last_trace;
static bool lcd_scope_last_trace_valid;
static bool lcd_scope_last_no_signal;
static scope_square_wave_estimate_t lcd_input_last_estimate;
static uint32_t lcd_input_last_estimate_ms;
static uint32_t lcd_input_last_estimate_config_hz;
static uint8_t lcd_input_estimate_miss_count;

/* Apollo F429 将 LTDC 单层帧缓冲直接映射到外部 SDRAM 首地址。 */
static uint16_t *const framebuffer = (uint16_t *)APP_LCD_FRAMEBUFFER_ADDRESS;

/* 维护一份简短状态文本，供 UART 状态行和启动日志复用。 */
static void lcd_set_state(const char *state) {
  (void)snprintf(lcd_state, sizeof(lcd_state), "%s", state);
}

/* 将 8 bit RGB 压缩为 LCD 当前使用的 RGB565 颜色格式。 */
static uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
  return (uint16_t)(((red & 0xF8U) << 8) | ((green & 0xFCU) << 3) | (blue >> 3));
}

/* 对帧缓冲做边界保护后的单点写入。 */
static void lcd_plot(uint16_t x, uint16_t y, uint16_t color) {
  if (x >= APP_LCD_WIDTH || y >= APP_LCD_HEIGHT) {
    return;
  }

  framebuffer[(y * APP_LCD_WIDTH) + x] = color;
}

/* 纯 CPU 填充矩形区域，用于当前最小 LCD 后端的所有绘制操作。 */
static void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
  for (uint16_t row = 0; row < height; ++row) {
    if ((y + row) >= APP_LCD_HEIGHT) {
      break;
    }
    for (uint16_t col = 0; col < width; ++col) {
      if ((x + col) >= APP_LCD_WIDTH) {
        break;
      }
      lcd_plot((uint16_t)(x + col), (uint16_t)(y + row), color);
    }
  }
}

/* 仅提供项目当前状态页需要的最小 ASCII 点阵字库。 */
static const uint8_t *glyph_for_char(char ch) {
  static const uint8_t blank[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
  static const uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static const uint8_t plus[5] = {0x08, 0x08, 0x3E, 0x08, 0x08};
  static const uint8_t period[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
  static const uint8_t slash[5] = {0x20, 0x10, 0x08, 0x04, 0x02};
  static const uint8_t colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
  static const uint8_t percent[5] = {0x62, 0x64, 0x08, 0x13, 0x23};
  static const uint8_t equal[5] = {0x14, 0x14, 0x14, 0x14, 0x14};
  static const uint8_t zero[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
  static const uint8_t one[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
  static const uint8_t two[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
  static const uint8_t three[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
  static const uint8_t four[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
  static const uint8_t five[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
  static const uint8_t six[5] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
  static const uint8_t seven[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
  static const uint8_t eight[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
  static const uint8_t nine[5] = {0x06, 0x49, 0x49, 0x29, 0x1E};
  static const uint8_t a[5] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
  static const uint8_t b[5] = {0x7F, 0x49, 0x49, 0x49, 0x36};
  static const uint8_t c[5] = {0x3E, 0x41, 0x41, 0x41, 0x22};
  static const uint8_t d[5] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
  static const uint8_t e[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
  static const uint8_t f[5] = {0x7F, 0x09, 0x09, 0x09, 0x01};
  static const uint8_t g[5] = {0x3E, 0x41, 0x49, 0x49, 0x7A};
  static const uint8_t h[5] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
  static const uint8_t i[5] = {0x00, 0x41, 0x7F, 0x41, 0x00};
  static const uint8_t j[5] = {0x20, 0x40, 0x41, 0x3F, 0x01};
  static const uint8_t k[5] = {0x7F, 0x08, 0x14, 0x22, 0x41};
  static const uint8_t l[5] = {0x7F, 0x40, 0x40, 0x40, 0x40};
  static const uint8_t m[5] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
  static const uint8_t n[5] = {0x7F, 0x04, 0x08, 0x10, 0x7F};
  static const uint8_t o[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
  static const uint8_t p[5] = {0x7F, 0x09, 0x09, 0x09, 0x06};
  static const uint8_t q[5] = {0x3E, 0x41, 0x51, 0x21, 0x5E};
  static const uint8_t r[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
  static const uint8_t s[5] = {0x46, 0x49, 0x49, 0x49, 0x31};
  static const uint8_t t[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
  static const uint8_t u[5] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
  static const uint8_t v[5] = {0x1F, 0x20, 0x40, 0x20, 0x1F};
  static const uint8_t w[5] = {0x7F, 0x20, 0x18, 0x20, 0x7F};
  static const uint8_t x[5] = {0x63, 0x14, 0x08, 0x14, 0x63};
  static const uint8_t y[5] = {0x03, 0x04, 0x78, 0x04, 0x03};
  static const uint8_t z[5] = {0x61, 0x51, 0x49, 0x45, 0x43};

  switch (ch) {
    case 'A': return a;
    case 'B': return b;
    case 'C': return c;
    case 'D': return d;
    case 'E': return e;
    case 'F': return f;
    case 'G': return g;
    case 'H': return h;
    case 'I': return i;
    case 'J': return j;
    case 'K': return k;
    case 'L': return l;
    case 'M': return m;
    case 'N': return n;
    case 'O': return o;
    case 'P': return p;
    case 'Q': return q;
    case 'R': return r;
    case 'S': return s;
    case 'T': return t;
    case 'U': return u;
    case 'V': return v;
    case 'W': return w;
    case 'X': return x;
    case 'Y': return y;
    case 'Z': return z;
    case '0': return zero;
    case '1': return one;
    case '2': return two;
    case '3': return three;
    case '4': return four;
    case '5': return five;
    case '6': return six;
    case '7': return seven;
    case '8': return eight;
    case '9': return nine;
    case '-': return dash;
    case '+': return plus;
    case '.': return period;
    case '/': return slash;
    case ':': return colon;
    case '%': return percent;
    case '=': return equal;
    case ' ':
    default:
      return blank;
  }
}

/* 按固定 5x7 字模绘制一个字符。 */
static void lcd_draw_char(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg, uint8_t scale) {
  const uint8_t *glyph = glyph_for_char(ch);

  for (uint8_t col = 0; col < 5; ++col) {
    for (uint8_t row = 0; row < 7; ++row) {
      uint16_t color = ((glyph[col] >> row) & 0x01U) != 0U ? fg : bg;
      lcd_fill_rect((uint16_t)(x + (col * scale)),
                    (uint16_t)(y + (row * scale)),
                    scale,
                    scale,
                    color);
    }
  }

  lcd_fill_rect((uint16_t)(x + (5U * scale)), y, scale, (uint16_t)(7U * scale), bg);
}

/* 绘制一串 ASCII 文本。 */
static void lcd_draw_string(uint16_t x, uint16_t y, const char *text, uint16_t fg, uint16_t bg, uint8_t scale) {
  const uint16_t advance = (uint16_t)(6U * scale);
  while (*text != '\0') {
    lcd_draw_char(x, y, *text, fg, bg, scale);
    x = (uint16_t)(x + advance);
    ++text;
  }
}

static void lcd_draw_box(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t border, uint16_t fill) {
  lcd_fill_rect(x, y, width, height, border);
  if (width > 2U && height > 2U) {
    lcd_fill_rect((uint16_t)(x + 1U), (uint16_t)(y + 1U), (uint16_t)(width - 2U), (uint16_t)(height - 2U), fill);
  }
}

static void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t width, uint16_t color) {
  lcd_fill_rect(x, y, width, 1U, color);
}

static void lcd_draw_vline(uint16_t x, uint16_t y, uint16_t height, uint16_t color) {
  lcd_fill_rect(x, y, 1U, height, color);
}

static void lcd_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
  int32_t dx = (int32_t)x1 - (int32_t)x0;
  int32_t sx = x0 < x1 ? 1 : -1;
  int32_t dy = -((int32_t)y1 - (int32_t)y0 < 0 ? -((int32_t)y1 - (int32_t)y0) : ((int32_t)y1 - (int32_t)y0));
  int32_t sy = y0 < y1 ? 1 : -1;
  int32_t err = dx + dy;

  while (1) {
    lcd_plot(x0, y0, color);
    if (x0 == x1 && y0 == y1) {
      break;
    }

    int32_t e2 = err * 2;
    if (e2 >= dy) {
      err += dy;
      x0 = (uint16_t)((int32_t)x0 + sx);
    }
    if (e2 <= dx) {
      err += dx;
      y0 = (uint16_t)((int32_t)y0 + sy);
    }
  }
}

static void lcd_draw_button(uint16_t x,
                            uint16_t y,
                            uint16_t width,
                            uint16_t height,
                            const char *label,
                            bool highlighted) {
  uint16_t border = highlighted ? rgb565(255, 216, 118) : rgb565(100, 208, 255);
  uint16_t fill = highlighted ? rgb565(74, 48, 8) : rgb565(17, 42, 82);
  uint16_t text = rgb565(234, 246, 255);
  uint16_t glow = highlighted ? rgb565(255, 176, 72) : rgb565(48, 92, 148);
  uint16_t text_x = (uint16_t)(x + (width >= 84U ? 12U : 8U));
  uint16_t text_y = (uint16_t)(y + (height >= 40U ? 12U : 9U));
  uint8_t scale = width >= 84U ? 2U : 1U;

  lcd_fill_rect((uint16_t)(x - 2U), (uint16_t)(y - 2U), (uint16_t)(width + 4U), (uint16_t)(height + 4U), glow);
  lcd_draw_box(x, y, width, height, border, fill);
  lcd_draw_string(text_x, text_y, label, text, fill, scale);
}

typedef struct {
  ui_highlight_t id;
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
  const char *label;
} lcd_button_def_t;

static const lcd_button_def_t lcd_buttons[] = {
    {UI_HIGHLIGHT_FREQ_DOWN, 22U, 224U, 72U, 30U, "F-1K"},
    {UI_HIGHLIGHT_FREQ_UP, 98U, 224U, 72U, 30U, "F+1K"},
    {UI_HIGHLIGHT_DUTY_DOWN, 174U, 224U, 72U, 30U, "D-5"},
    {UI_HIGHLIGHT_DUTY_UP, 250U, 224U, 72U, 30U, "D+5"},
    {UI_HIGHLIGHT_RESET, 356U, 224U, 98U, 30U, "RESET"},
    {UI_HIGHLIGHT_HELP, 360U, 14U, 98U, 28U, "MORE"},
};

static const lcd_button_def_t *lcd_find_button(ui_highlight_t id) {
  for (size_t i = 0; i < (sizeof(lcd_buttons) / sizeof(lcd_buttons[0])); ++i) {
    if (lcd_buttons[i].id == id) {
      return &lcd_buttons[i];
    }
  }

  return NULL;
}

static void lcd_draw_scope_trace(const scope_capture_snapshot_t *snapshot);
static void lcd_draw_scope_static_plot_frame(void);
static void lcd_restore_scope_columns(uint16_t x_begin, uint16_t x_end);
static uint32_t lcd_scope_adc_sample_rate_hz(void);

/* 仅重绘发生状态变化的单个按钮，避免整页按钮区重复闪烁。 */
static void lcd_redraw_button(ui_highlight_t id, bool highlighted) {
  const lcd_button_def_t *button = lcd_find_button(id);

  if (button == NULL) {
    return;
  }

  lcd_fill_rect((uint16_t)(button->x - 2U),
                (uint16_t)(button->y - 2U),
                (uint16_t)(button->width + 4U),
                (uint16_t)(button->height + 4U),
                rgb565(7, 16, 34));
  lcd_draw_button(button->x,
                  button->y,
                  button->width,
                  button->height,
                  button->label,
                  highlighted);
}

/* 用深色背景清空整块显示区域。 */
static void lcd_clear(void) {
  lcd_fill_rect(0, 0, APP_LCD_WIDTH, APP_LCD_HEIGHT, rgb565(8, 18, 42));
}

/* 上电后先做最小 SDRAM 读写探针，排除帧缓冲地址或 FMC 初始化错误。 */
static bool lcd_sdram_probe(void) {
  static const uint16_t pattern[] = {
      0xF800U, 0x07E0U, 0x001FU, 0xFFFFU,
      0x0000U, 0xA145U, 0x5AA5U, 0x1357U,
  };

  for (size_t i = 0; i < (sizeof(pattern) / sizeof(pattern[0])); ++i) {
    framebuffer[i] = pattern[i];
  }

  for (size_t i = 0; i < (sizeof(pattern) / sizeof(pattern[0])); ++i) {
    if (framebuffer[i] != pattern[i]) {
      return false;
    }
  }

  return true;
}

/* 启动阶段显示四色测试图，便于人工确认 LTDC 输出已经打通。 */
static void lcd_fill_test_pattern(void) {
  uint16_t half_w = APP_LCD_WIDTH / 2U;
  uint16_t half_h = APP_LCD_HEIGHT / 2U;

  lcd_fill_rect(0, 0, half_w, half_h, rgb565(255, 0, 0));
  lcd_fill_rect(half_w, 0, (uint16_t)(APP_LCD_WIDTH - half_w), half_h, rgb565(0, 255, 0));
  lcd_fill_rect(0, half_h, half_w, (uint16_t)(APP_LCD_HEIGHT - half_h), rgb565(0, 0, 255));
  lcd_fill_rect(half_w, half_h, (uint16_t)(APP_LCD_WIDTH - half_w), (uint16_t)(APP_LCD_HEIGHT - half_h), rgb565(255, 255, 0));

  lcd_fill_rect(220, 116, 40, 40, rgb565(255, 255, 255));
  lcd_fill_rect(228, 124, 24, 24, rgb565(0, 0, 0));
}

/* 将当前 PWM 设定和 ADC 示波状态格式化成 LCD 页面文本。 */
static void lcd_format_status(lcd_status_view_t *view,
                              const signal_gen_config_t *config,
                              const signal_measure_result_t *measurement) {
  const scope_capture_snapshot_t *snapshot = signal_capture_adc_latest();
  (void)measurement;

  (void)snprintf(view->header, sizeof(view->header), "REAL-TIME WAVEFORM");
  (void)snprintf(view->set_line,
                 sizeof(view->set_line),
                 "SET F=%" PRIu32 "HZ D=%u%%",
                 config->frequency_hz,
                 config->duty_percent);

  if (snapshot != NULL && snapshot->valid) {
    (void)snprintf(view->meas_line,
                   sizeof(view->meas_line),
                   "ADC MIN=%u MAX=%u AVG=%u",
                   snapshot->min_raw,
                   snapshot->max_raw,
                   snapshot->mean_raw);
    (void)snprintf(view->err_line,
                   sizeof(view->err_line),
                   "SCOPE READY  PB6 -> PA0");
    return;
  }

  (void)snprintf(view->meas_line, sizeof(view->meas_line), "ADC NO-SIGNAL");
  (void)snprintf(view->err_line, sizeof(view->err_line), "CHECK PB6-PA0 LOOPBACK");
}

static void lcd_draw_card(uint16_t x,
                          uint16_t y,
                          uint16_t width,
                          uint16_t height,
                          uint16_t border,
                          uint16_t fill,
                          uint16_t accent) {
  lcd_fill_rect(x, y, width, height, border);
  lcd_fill_rect((uint16_t)(x + 2U), (uint16_t)(y + 2U), (uint16_t)(width - 4U), (uint16_t)(height - 4U), fill);
  lcd_fill_rect((uint16_t)(x + 2U), (uint16_t)(y + 2U), (uint16_t)(width - 4U), 4U, accent);
}

static void lcd_draw_splash(void) {
  uint16_t bg = rgb565(6, 16, 36);
  uint16_t accent = rgb565(87, 212, 255);
  uint16_t accent2 = rgb565(255, 180, 92);
  uint16_t panel = rgb565(12, 30, 62);
  uint16_t text = rgb565(236, 246, 255);
  uint16_t shadow = rgb565(18, 55, 92);

  lcd_clear();
  lcd_fill_rect(0, 0, APP_LCD_WIDTH, APP_LCD_HEIGHT, bg);
  lcd_fill_rect(0, 0, APP_LCD_WIDTH, 18U, shadow);
  lcd_fill_rect(0, (uint16_t)(APP_LCD_HEIGHT - 14U), APP_LCD_WIDTH, 14U, shadow);
  lcd_fill_rect(24, 28, 432, 178, panel);
  lcd_fill_rect(32, 36, 416, 8, accent);
  lcd_fill_rect(32, 52, 144, 6, accent2);
  lcd_fill_rect(304, 180, 120, 6, accent2);
  lcd_draw_hline(40, 170, 220, shadow);
  lcd_draw_vline(250, 82, 66, shadow);

  lcd_draw_string(38, 72, "HEZZZ/STM32-SIGNAL-GENERATOR", text, panel, 2);
  lcd_draw_string(74, 112, "PWM + MEASURE + TOUCH", accent, panel, 2);
  lcd_draw_string(126, 152, "SEE GITHUB REPO", accent2, panel, 2);
  lcd_draw_string(140, 222, "APOLLO F429 RGBLCD DEMO", text, bg, 1);
}

static void lcd_draw_control_static(const ui_ctrl_view_t *ui) {
  uint16_t bg = rgb565(7, 16, 34);
  uint16_t chrome = rgb565(20, 48, 86);
  uint16_t panel = rgb565(14, 28, 56);
  uint16_t border = rgb565(54, 122, 178);

  lcd_fill_rect(0, 0, APP_LCD_WIDTH, APP_LCD_HEIGHT, bg);
  lcd_fill_rect(0, 0, APP_LCD_WIDTH, 12U, chrome);
  lcd_fill_rect(0, (uint16_t)(APP_LCD_HEIGHT - 10U), APP_LCD_WIDTH, 10U, chrome);
  lcd_draw_hline(18, 46, 444, border);
  lcd_draw_card(LCD_SCOPE_X, LCD_SCOPE_Y, LCD_SCOPE_WIDTH, LCD_SCOPE_HEIGHT, border, panel, rgb565(85, 210, 255));
  lcd_draw_scope_static_plot_frame();
  for (size_t i = 0; i < (sizeof(lcd_buttons) / sizeof(lcd_buttons[0])); ++i) {
    lcd_draw_button(lcd_buttons[i].x,
                    lcd_buttons[i].y,
                    lcd_buttons[i].width,
                    lcd_buttons[i].height,
                    lcd_buttons[i].label,
                    ui->highlight == lcd_buttons[i].id);
  }
}

static void lcd_draw_footer(const ui_ctrl_view_t *ui) {
  uint16_t warn = rgb565(255, 208, 116);
  uint16_t text = rgb565(236, 246, 255);
  uint16_t footer_bg = rgb565(10, 22, 46);
  char touch_line[40];

  if (!ui->touch_ready) {
    (void)snprintf(touch_line, sizeof(touch_line), "TOUCH INIT...");
  } else if (ui->last_touch.x != 0U || ui->last_touch.y != 0U) {
    (void)snprintf(touch_line, sizeof(touch_line), "TOUCH X=%u Y=%u", ui->last_touch.x, ui->last_touch.y);
  } else {
    (void)snprintf(touch_line, sizeof(touch_line), "TOUCH READY");
  }

  lcd_fill_rect(18, 260, 444, 10, footer_bg);
  lcd_draw_string(22, 260, touch_line, text, footer_bg, 1);
  lcd_draw_string(250, 260, ui->footer, warn, footer_bg, 1);
}

static uint32_t lcd_scope_adc_sample_rate_hz(void) {
  uint32_t pclk2_hz = HAL_RCC_GetPCLK2Freq();

  return pclk2_hz / 4U / 96U;
}

static void lcd_draw_info_cards(const signal_gen_config_t *config,
                                const signal_measure_result_t *measurement,
                                const scope_capture_snapshot_t *snapshot) {
  uint16_t panel = rgb565(7, 16, 34);
  uint16_t border = rgb565(54, 122, 178);
  uint16_t input_accent = rgb565(112, 232, 162);
  uint16_t output_accent = rgb565(255, 182, 86);
  uint16_t text = rgb565(236, 246, 255);
  uint16_t warn = rgb565(255, 208, 116);
  char line[40];
  scope_square_wave_estimate_t estimate = {0};
  uint32_t min_frequency_hz = 0U;
  uint32_t max_frequency_hz = 0U;
  uint32_t adc_sample_rate_hz = lcd_scope_adc_sample_rate_hz();
  uint32_t now_ms = HAL_GetTick();
  bool duty_frequency_window_valid = scope_square_wave_frequency_window(APP_SCOPE_SAMPLE_COUNT,
                                                                        adc_sample_rate_hz,
                                                                        &min_frequency_hz,
                                                                        &max_frequency_hz);
  bool within_duty_window = duty_frequency_window_valid &&
                            config->frequency_hz >= min_frequency_hz &&
                            config->frequency_hz <= max_frequency_hz;
  bool square_signal_present = snapshot != NULL && snapshot->valid &&
                               scope_square_wave_signal_present(snapshot->samples,
                                                                snapshot->sample_count,
                                                                APP_SCOPE_SIGNAL_FLAT_THRESHOLD,
                                                                APP_SCOPE_SIGNAL_MIN_SPAN,
                                                                APP_SCOPE_SQUARE_LOW_LEVEL_MAX,
                                                                APP_SCOPE_SQUARE_HIGH_LEVEL_MIN);
  bool can_hold_last_estimate = lcd_input_last_estimate.valid &&
                                lcd_input_last_estimate_config_hz == config->frequency_hz &&
                                (now_ms - lcd_input_last_estimate_ms) <= APP_SCOPE_ESTIMATE_HOLD_MS;
  bool can_tolerate_estimate_miss = lcd_input_last_estimate.valid &&
                                    lcd_input_last_estimate_config_hz == config->frequency_hz &&
                                    lcd_input_estimate_miss_count < APP_SCOPE_ESTIMATE_MISS_MAX;
  (void)measurement;

  if (square_signal_present && within_duty_window) {
    (void)scope_estimate_square_wave(snapshot->samples,
                                     snapshot->sample_count,
                                     APP_SCOPE_SIGNAL_FLAT_THRESHOLD,
                                     adc_sample_rate_hz,
                                     &estimate);
    if (estimate.valid) {
      lcd_input_last_estimate = estimate;
      lcd_input_last_estimate_ms = now_ms;
      lcd_input_last_estimate_config_hz = config->frequency_hz;
      lcd_input_estimate_miss_count = 0U;
    } else if (lcd_input_last_estimate.valid &&
               lcd_input_last_estimate_config_hz == config->frequency_hz &&
               lcd_input_estimate_miss_count < 0xFFU) {
      ++lcd_input_estimate_miss_count;
    }
  } else {
    lcd_input_estimate_miss_count = 0U;
  }

  lcd_draw_card(18, 14, 156, 28, border, panel, input_accent);
  lcd_draw_card(182, 14, 156, 28, border, panel, output_accent);

  lcd_draw_string(26, 20, "INPUT", text, panel, 1);
  lcd_draw_string(68, 20, "PA0 ADC1", input_accent, panel, 1);
  if (estimate.valid) {
    (void)snprintf(line, sizeof(line), "F=%luHZ D=%u%%", estimate.frequency_hz, estimate.duty_percent);
    lcd_draw_string(26, 30, line, text, panel, 1);
  } else if (can_hold_last_estimate || can_tolerate_estimate_miss) {
    (void)snprintf(line,
                   sizeof(line),
                   "F=%luHZ D=%u%%",
                   lcd_input_last_estimate.frequency_hz,
                   lcd_input_last_estimate.duty_percent);
    lcd_draw_string(26, 30, line, text, panel, 1);
  } else if (square_signal_present) {
    if (within_duty_window) {
      lcd_draw_string(26, 30, "F=ADC LIVE D=--", warn, panel, 1);
    } else {
      (void)snprintf(line, sizeof(line), "F=%luHZ D=--", config->frequency_hz);
      lcd_draw_string(26, 30, line, warn, panel, 1);
    }
  } else {
    lcd_draw_string(26, 30, "NO INPUT", warn, panel, 1);
  }

  lcd_draw_string(190, 20, "OUTPUT", text, panel, 1);
  lcd_draw_string(238, 20, "PB6 TIM4", output_accent, panel, 1);
  (void)snprintf(line, sizeof(line), "F=%luHZ D=%u%%", config->frequency_hz, config->duty_percent);
  lcd_draw_string(190, 30, line, text, panel, 1);
}

static void lcd_draw_scope_static_plot_frame(void) {
  uint16_t bg = rgb565(10, 20, 42);
  uint16_t grid = rgb565(32, 66, 108);

  lcd_fill_rect(LCD_SCOPE_PLOT_X, LCD_SCOPE_PLOT_Y, LCD_SCOPE_PLOT_WIDTH, LCD_SCOPE_PLOT_HEIGHT, bg);
  lcd_draw_box(LCD_SCOPE_PLOT_X, LCD_SCOPE_PLOT_Y, LCD_SCOPE_PLOT_WIDTH, LCD_SCOPE_PLOT_HEIGHT, grid, bg);
  lcd_draw_hline(LCD_SCOPE_PLOT_X,
                 (uint16_t)(LCD_SCOPE_PLOT_Y + (LCD_SCOPE_PLOT_HEIGHT / 2U)),
                 LCD_SCOPE_PLOT_WIDTH,
                 grid);
  for (uint16_t x = (uint16_t)(LCD_SCOPE_PLOT_X + 53U); x < (LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH); x = (uint16_t)(x + 53U)) {
    lcd_draw_vline(x, LCD_SCOPE_PLOT_Y, LCD_SCOPE_PLOT_HEIGHT, grid);
  }
}

static void lcd_restore_scope_columns(uint16_t x_begin, uint16_t x_end) {
  uint16_t bg = rgb565(10, 20, 42);
  uint16_t grid = rgb565(32, 66, 108);
  uint16_t start = x_begin < LCD_SCOPE_PLOT_X ? LCD_SCOPE_PLOT_X : x_begin;
  uint16_t end = x_end >= (LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH) ?
                 (uint16_t)(LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH - 1U) : x_end;
  uint16_t width;

  if (end < start) {
    return;
  }

  width = (uint16_t)(end - start + 1U);
  lcd_fill_rect(start, LCD_SCOPE_PLOT_Y, width, LCD_SCOPE_PLOT_HEIGHT, bg);
  lcd_draw_hline(start,
                 (uint16_t)(LCD_SCOPE_PLOT_Y + (LCD_SCOPE_PLOT_HEIGHT / 2U)),
                 width,
                 grid);
  for (uint16_t x = start; x <= end; ++x) {
    if (x == LCD_SCOPE_PLOT_X ||
        x == (uint16_t)(LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH - 1U) ||
        ((uint16_t)(x - LCD_SCOPE_PLOT_X) % 53U) == 0U) {
      lcd_draw_vline(x, LCD_SCOPE_PLOT_Y, LCD_SCOPE_PLOT_HEIGHT, grid);
    }
  }
}

static void lcd_draw_more_overlay(void) {
  uint16_t panel = rgb565(10, 24, 52);
  uint16_t border = rgb565(110, 196, 255);
  uint16_t accent = rgb565(255, 190, 84);
  uint16_t text = rgb565(238, 246, 255);
  uint16_t link = rgb565(120, 224, 255);

  lcd_draw_card(LCD_MORE_X, LCD_MORE_Y, LCD_MORE_WIDTH, LCD_MORE_HEIGHT, border, panel, accent);

  lcd_draw_string((uint16_t)(LCD_MORE_X + 16U), (uint16_t)(LCD_MORE_Y + 16U), "HEZZZ/STM32-SIGNAL-GENERATOR", text, panel, 1);
  lcd_draw_string((uint16_t)(LCD_MORE_X + 34U), (uint16_t)(LCD_MORE_Y + 34U), "PWM + MEASURE + TOUCH DEMO", accent, panel, 1);
  lcd_draw_hline((uint16_t)(LCD_MORE_X + 16U), (uint16_t)(LCD_MORE_Y + 54U), 304U, rgb565(34, 72, 118));

  lcd_draw_string((uint16_t)(LCD_MORE_X + 16U), (uint16_t)(LCD_MORE_Y + 66U), "REPO HEZ2Z/STM32-SIGNAL-GENERATOR", link, panel, 1);
  lcd_draw_string((uint16_t)(LCD_MORE_X + 16U), (uint16_t)(LCD_MORE_Y + 80U), "METER", link, panel, 1);
  lcd_draw_string((uint16_t)(LCD_MORE_X + 16U), (uint16_t)(LCD_MORE_Y + 98U), "BOARD APOLLO STM32F429IGT6", text, panel, 1);
  lcd_draw_string((uint16_t)(LCD_MORE_X + 16U), (uint16_t)(LCD_MORE_Y + 112U), "LCD ALIENTEK 4342 RGBLCD + GT9147", text, panel, 1);
  lcd_draw_string((uint16_t)(LCD_MORE_X + 16U), (uint16_t)(LCD_MORE_Y + 126U), "SCOPE PB6 TIM4 CH1 -> PA0 ADC1 IN0", text, panel, 1);
  lcd_draw_string((uint16_t)(LCD_MORE_X + 16U), (uint16_t)(LCD_MORE_Y + 140U), "TAP MORE OR BLANK AREA TO CLOSE", accent, panel, 1);
}

static void lcd_restore_more_overlay(const ui_ctrl_view_t *ui,
                                     const lcd_status_view_t *status_view,
                                     const signal_measure_result_t *measurement) {
  uint16_t bg = rgb565(7, 16, 34);
  uint16_t border = rgb565(54, 122, 178);
  uint16_t panel = rgb565(14, 28, 56);
  (void)status_view;

  lcd_fill_rect(LCD_MORE_X, LCD_MORE_Y, LCD_MORE_WIDTH, LCD_MORE_HEIGHT, bg);
  lcd_draw_hline(18, 46, 444, border);
  lcd_draw_card(LCD_SCOPE_X, LCD_SCOPE_Y, LCD_SCOPE_WIDTH, LCD_SCOPE_HEIGHT, border, panel, rgb565(85, 210, 255));
  lcd_draw_scope_static_plot_frame();

  for (size_t i = 0; i < (sizeof(lcd_buttons) / sizeof(lcd_buttons[0])); ++i) {
    lcd_draw_button(lcd_buttons[i].x,
                    lcd_buttons[i].y,
                    lcd_buttons[i].width,
                    lcd_buttons[i].height,
                    lcd_buttons[i].label,
                    ui->highlight == lcd_buttons[i].id);
  }

  lcd_draw_info_cards(&ui->pending_config, measurement, signal_capture_adc_latest());
  lcd_draw_scope_trace(signal_capture_adc_latest());
}

static void lcd_draw_scope_trace(const scope_capture_snapshot_t *snapshot) {
  scope_render_trace_t trace = {0};
  uint16_t trace_color = rgb565(112, 232, 162);
  uint16_t warn = rgb565(255, 208, 116);
  char label[32];
  bool no_signal = false;

  if (snapshot == NULL || !snapshot->valid ||
      !scope_render_build_trace(snapshot->samples,
                                snapshot->sample_count,
                                LCD_SCOPE_PLOT_HEIGHT,
                                APP_SCOPE_SIGNAL_FLAT_THRESHOLD,
                                &trace) ||
      trace.flat_signal) {
    no_signal = true;
  }

  if (no_signal) {
    if (!lcd_scope_last_no_signal) {
      lcd_draw_scope_static_plot_frame();
    }
    lcd_draw_string((uint16_t)(LCD_SCOPE_PLOT_X + 86U),
                    (uint16_t)(LCD_SCOPE_PLOT_Y + 36U),
                    "NO SIGNAL ON PA0",
                    warn,
                    rgb565(10, 20, 42),
                    2);
    lcd_scope_last_trace_valid = false;
    lcd_scope_last_no_signal = true;
    return;
  }

  if (!lcd_scope_last_trace_valid || lcd_scope_last_no_signal) {
    lcd_draw_scope_static_plot_frame();
  } else {
    uint16_t first_changed = 0U;
    uint16_t last_changed = 0U;
    bool changed = false;

    for (uint16_t i = 0; i < trace.point_count; ++i) {
      if (i >= lcd_scope_last_trace.point_count ||
          lcd_scope_last_trace.y_points[i] != trace.y_points[i]) {
        if (!changed) {
          first_changed = i;
          changed = true;
        }
        last_changed = i;
      }
    }

    if (changed) {
      uint16_t x_begin = (uint16_t)(LCD_SCOPE_PLOT_X +
                        (((uint32_t)first_changed * (LCD_SCOPE_PLOT_WIDTH - 1U)) / (trace.point_count - 1U)));
      uint16_t x_end = (uint16_t)(LCD_SCOPE_PLOT_X +
                      (((uint32_t)last_changed * (LCD_SCOPE_PLOT_WIDTH - 1U)) / (trace.point_count - 1U)));
      x_begin = x_begin > (LCD_SCOPE_PLOT_X + 2U) ? (uint16_t)(x_begin - 2U) : LCD_SCOPE_PLOT_X;
      x_end = (uint16_t)(x_end + 2U);
      lcd_restore_scope_columns(x_begin, x_end);
    }
  }

  for (uint16_t i = 1; i < trace.point_count; ++i) {
    uint16_t x0 = (uint16_t)(LCD_SCOPE_PLOT_X + (((uint32_t)(i - 1U) * (LCD_SCOPE_PLOT_WIDTH - 1U)) / (trace.point_count - 1U)));
    uint16_t x1 = (uint16_t)(LCD_SCOPE_PLOT_X + (((uint32_t)i * (LCD_SCOPE_PLOT_WIDTH - 1U)) / (trace.point_count - 1U)));
    uint16_t y0 = (uint16_t)(LCD_SCOPE_PLOT_Y + trace.y_points[i - 1U]);
    uint16_t y1 = (uint16_t)(LCD_SCOPE_PLOT_Y + trace.y_points[i]);
    lcd_draw_line(x0, y0, x1, y1, trace_color);
  }

  (void)snprintf(label, sizeof(label), "N=%u", trace.point_count);
  lcd_draw_string((uint16_t)(LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH - 44U),
                  (uint16_t)(LCD_SCOPE_PLOT_Y + 4U),
                  label,
                  rgb565(236, 246, 255),
                  rgb565(10, 20, 42),
                  1);
  lcd_scope_last_trace = trace;
  lcd_scope_last_trace_valid = true;
  lcd_scope_last_no_signal = false;
}

static void lcd_draw_control_dynamic(const ui_ctrl_view_t *ui,
                                     const lcd_status_view_t *status_view) {
  const scope_capture_snapshot_t *snapshot = signal_capture_adc_latest();
  const signal_measure_result_t *measurement = signal_measure_latest();
  (void)status_view;
  lcd_draw_scope_trace(snapshot);
  lcd_draw_info_cards(&ui->pending_config, measurement, snapshot);
  lcd_draw_footer(ui);
}

static void lcd_render_ui(const ui_ctrl_view_t *ui,
                          const signal_gen_config_t *config,
                          const signal_measure_result_t *measurement) {
  lcd_status_view_t status_view = {0};

  lcd_format_status(&status_view, config, measurement);
  if (!ui->touch_ready) {
    (void)snprintf(status_view.touch_line, sizeof(status_view.touch_line), "%s", "TOUCH INIT...");
  } else if (ui->last_touch.x != 0U || ui->last_touch.y != 0U) {
    (void)snprintf(status_view.touch_line,
                   sizeof(status_view.touch_line),
                   "TOUCH X=%u Y=%u",
                   ui->last_touch.x,
                   ui->last_touch.y);
  } else {
    (void)snprintf(status_view.touch_line, sizeof(status_view.touch_line), "%s", "TOUCH READY");
  }
  (void)snprintf(status_view.hint_line,
                 sizeof(status_view.hint_line),
                 "UART HELP / STATUS / FREQ / DUTY");

  bool scene_changed = !lcd_ui_last.valid || lcd_ui_last.scene != lcd_scene;
  bool highlight_changed = !scene_changed && lcd_ui_last.highlight != ui->highlight;
  bool title_changed = scene_changed || strcmp(lcd_ui_last.title, ui->title) != 0;
  bool more_changed = scene_changed || lcd_ui_last.more_open != ui->more_open;
  bool footer_changed = scene_changed || strcmp(lcd_ui_last.footer, ui->footer) != 0;
  bool touch_changed = scene_changed || strcmp(lcd_ui_last.touch_line, status_view.touch_line) != 0;
  bool freq_changed = scene_changed ||
                      lcd_ui_last.pending_config.frequency_hz != ui->pending_config.frequency_hz;
  bool duty_changed = scene_changed ||
                      lcd_ui_last.pending_config.duty_percent != ui->pending_config.duty_percent;
  bool set_changed = scene_changed || strcmp(lcd_ui_last.set_line, status_view.set_line) != 0;
  bool meas_changed = scene_changed || strcmp(lcd_ui_last.meas_line, status_view.meas_line) != 0;
  bool err_changed = scene_changed || strcmp(lcd_ui_last.err_line, status_view.err_line) != 0;
  bool hint_changed = scene_changed || strcmp(lcd_ui_last.hint_line, status_view.hint_line) != 0;
  bool needs_dynamic = title_changed || more_changed || footer_changed || touch_changed || freq_changed ||
                       duty_changed || set_changed || meas_changed || err_changed || hint_changed;

  if (scene_changed && lcd_scene == LCD_SCENE_SPLASH) {
    lcd_draw_splash();
  } else if (lcd_scene == LCD_SCENE_CONTROL) {
    uint32_t now = HAL_GetTick();
    bool overlay_open = ui->more_open;
    bool force_refresh = scene_changed || more_changed || highlight_changed || freq_changed || duty_changed ||
                         footer_changed || touch_changed;
    bool allow_measure_refresh = force_refresh ||
                                 (lcd_last_dynamic_refresh_ms == 0U) ||
                                 ((now - lcd_last_dynamic_refresh_ms) >= LCD_DYNAMIC_REFRESH_MS);

    if (scene_changed) {
      lcd_draw_control_static(ui);
      lcd_draw_control_dynamic(ui, &status_view);
      if (overlay_open) {
        lcd_draw_more_overlay();
      }
      lcd_last_dynamic_refresh_ms = now;
    } else {
      if (more_changed) {
        if (overlay_open) {
          lcd_redraw_button(UI_HIGHLIGHT_HELP, true);
          lcd_draw_more_overlay();
        } else {
          lcd_restore_more_overlay(ui, &status_view, measurement);
          lcd_redraw_button(UI_HIGHLIGHT_HELP, false);
        }
        lcd_last_dynamic_refresh_ms = now;
      } else if (highlight_changed && !overlay_open) {
        lcd_redraw_button(lcd_ui_last.highlight, false);
        lcd_redraw_button(ui->highlight, true);
      }

      if ((freq_changed || duty_changed || meas_changed || err_changed) && !overlay_open) {
        lcd_draw_info_cards(&ui->pending_config, measurement, signal_capture_adc_latest());
      }
      if (footer_changed || touch_changed) {
        lcd_draw_footer(ui);
      }
      if (allow_measure_refresh && !overlay_open) {
        lcd_draw_scope_trace(signal_capture_adc_latest());
        lcd_last_dynamic_refresh_ms = now;
      }
    }

    if (!force_refresh && !allow_measure_refresh &&
        (set_changed || meas_changed || err_changed)) {
      return;
    }
  }

  if (scene_changed || highlight_changed || needs_dynamic) {
    lcd_ui_last.valid = true;
    lcd_ui_last.scene = lcd_scene;
    lcd_ui_last.more_open = ui->more_open;
    lcd_ui_last.highlight = ui->highlight;
    lcd_ui_last.pending_config = ui->pending_config;
    (void)snprintf(lcd_ui_last.title, sizeof(lcd_ui_last.title), "%s", ui->title);
    (void)snprintf(lcd_ui_last.footer, sizeof(lcd_ui_last.footer), "%s", ui->footer);
    (void)snprintf(lcd_ui_last.set_line, sizeof(lcd_ui_last.set_line), "%s", status_view.set_line);
    (void)snprintf(lcd_ui_last.meas_line, sizeof(lcd_ui_last.meas_line), "%s", status_view.meas_line);
    (void)snprintf(lcd_ui_last.err_line, sizeof(lcd_ui_last.err_line), "%s", status_view.err_line);
    (void)snprintf(lcd_ui_last.touch_line, sizeof(lcd_ui_last.touch_line), "%s", status_view.touch_line);
    (void)snprintf(lcd_ui_last.hint_line, sizeof(lcd_ui_last.hint_line), "%s", status_view.hint_line);
  }
}

/* 按 SDRAM 数据手册要求发送上电初始化命令序列。 */
static HAL_StatusTypeDef lcd_sdram_startup_sequence(void) {
  FMC_SDRAM_CommandTypeDef command = {0};
  uint32_t mode = 0;
  uint32_t command_target = APP_SDRAM_BANK == FMC_SDRAM_BANK2
                                ? FMC_SDRAM_CMD_TARGET_BANK2
                                : FMC_SDRAM_CMD_TARGET_BANK1;

  command.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
  command.CommandTarget = command_target;
  command.AutoRefreshNumber = 1;
  command.ModeRegisterDefinition = 0;
  if (HAL_SDRAM_SendCommand(&sdram_handle, &command, APP_SDRAM_TIMEOUT) != HAL_OK) {
    return HAL_ERROR;
  }

  HAL_Delay(1);

  command.CommandMode = FMC_SDRAM_CMD_PALL;
  command.CommandTarget = command_target;
  if (HAL_SDRAM_SendCommand(&sdram_handle, &command, APP_SDRAM_TIMEOUT) != HAL_OK) {
    return HAL_ERROR;
  }

  command.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
  command.CommandTarget = command_target;
  command.AutoRefreshNumber = 8;
  if (HAL_SDRAM_SendCommand(&sdram_handle, &command, APP_SDRAM_TIMEOUT) != HAL_OK) {
    return HAL_ERROR;
  }

  mode = SDRAM_MODEREG_BURST_LENGTH_1 |
         SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL |
         SDRAM_MODEREG_CAS_LATENCY_3 |
         SDRAM_MODEREG_OPERATING_MODE_STANDARD |
         SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

  command.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
  command.CommandTarget = command_target;
  command.AutoRefreshNumber = 1;
  command.ModeRegisterDefinition = mode;
  if (HAL_SDRAM_SendCommand(&sdram_handle, &command, APP_SDRAM_TIMEOUT) != HAL_OK) {
    return HAL_ERROR;
  }

  if (HAL_SDRAM_ProgramRefreshRate(&sdram_handle, 1292U) != HAL_OK) {
    return HAL_ERROR;
  }

  return HAL_OK;
}

/* 初始化外部 SDRAM，使其可作为 LTDC 帧缓冲使用。 */
static HAL_StatusTypeDef lcd_init_sdram(void) {
  FMC_SDRAM_TimingTypeDef timing = {0};

  sdram_handle.Instance = FMC_SDRAM_DEVICE;
  sdram_handle.Init.SDBank = APP_SDRAM_BANK;
  sdram_handle.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_8;
  sdram_handle.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_12;
  sdram_handle.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_16;
  sdram_handle.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
  sdram_handle.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_3;
  sdram_handle.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  sdram_handle.Init.SDClockPeriod = FMC_SDRAM_CLOCK_PERIOD_2;
  sdram_handle.Init.ReadBurst = FMC_SDRAM_RBURST_ENABLE;
  sdram_handle.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_0;

  timing.LoadToActiveDelay = 2;
  timing.ExitSelfRefreshDelay = 6;
  timing.SelfRefreshTime = 4;
  timing.RowCycleDelay = 6;
  timing.WriteRecoveryTime = 2;
  timing.RPDelay = 2;
  timing.RCDDelay = 2;

  if (HAL_SDRAM_Init(&sdram_handle, &timing) != HAL_OK) {
    return HAL_ERROR;
  }

  return lcd_sdram_startup_sequence();
}

/* 依据 Apollo 4342 面板参数初始化 LTDC 单层输出。 */
static HAL_StatusTypeDef lcd_init_ltdc(void) {
  LTDC_LayerCfgTypeDef layer = {0};

  lcd_handle.Instance = LTDC;
  lcd_handle.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  lcd_handle.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  lcd_handle.Init.DEPolarity = LTDC_DEPOLARITY_AL;
  lcd_handle.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
  lcd_handle.Init.HorizontalSync = APP_LCD_HSYNC - 1U;
  lcd_handle.Init.VerticalSync = APP_LCD_VSYNC - 1U;
  lcd_handle.Init.AccumulatedHBP = APP_LCD_HSYNC + APP_LCD_HBP - 1U;
  lcd_handle.Init.AccumulatedVBP = APP_LCD_VSYNC + APP_LCD_VBP - 1U;
  lcd_handle.Init.AccumulatedActiveW = APP_LCD_HSYNC + APP_LCD_HBP + APP_LCD_WIDTH - 1U;
  lcd_handle.Init.AccumulatedActiveH = APP_LCD_VSYNC + APP_LCD_VBP + APP_LCD_HEIGHT - 1U;
  lcd_handle.Init.TotalWidth = APP_LCD_HSYNC + APP_LCD_HBP + APP_LCD_WIDTH + APP_LCD_HFP - 1U;
  lcd_handle.Init.TotalHeigh = APP_LCD_VSYNC + APP_LCD_VBP + APP_LCD_HEIGHT + APP_LCD_VFP - 1U;
  lcd_handle.Init.Backcolor.Red = 0;
  lcd_handle.Init.Backcolor.Green = 0;
  lcd_handle.Init.Backcolor.Blue = 0;

  if (HAL_LTDC_Init(&lcd_handle) != HAL_OK) {
    return HAL_ERROR;
  }

  layer.WindowX0 = 0;
  layer.WindowX1 = APP_LCD_WIDTH;
  layer.WindowY0 = 0;
  layer.WindowY1 = APP_LCD_HEIGHT;
  layer.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
  layer.FBStartAdress = APP_LCD_FRAMEBUFFER_ADDRESS;
  layer.Alpha = 255;
  layer.Alpha0 = 0;
  layer.Backcolor.Red = 0;
  layer.Backcolor.Green = 0;
  layer.Backcolor.Blue = 0;
  layer.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
  layer.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
  layer.ImageWidth = APP_LCD_WIDTH;
  layer.ImageHeight = APP_LCD_HEIGHT;

  if (HAL_LTDC_ConfigLayer(&lcd_handle, &layer, 0) != HAL_OK) {
    return HAL_ERROR;
  }

  return HAL_OK;
}

/* 初始化背光控制 GPIO，默认先关闭背光避免错误画面直出。 */
static void lcd_backlight_init(void) {
  GPIO_InitTypeDef gpio_init = {0};

  APP_LCD_BACKLIGHT_GPIO_CLK_ENABLE();

  gpio_init.Pin = APP_LCD_BACKLIGHT_PIN;
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(APP_LCD_BACKLIGHT_PORT, &gpio_init);

  HAL_GPIO_WritePin(APP_LCD_BACKLIGHT_PORT, APP_LCD_BACKLIGHT_PIN, GPIO_PIN_RESET);
}

/* 在显示链路准备完成后再点亮背光。 */
static void lcd_backlight_on(void) {
  HAL_GPIO_WritePin(APP_LCD_BACKLIGHT_PORT, APP_LCD_BACKLIGHT_PIN, GPIO_PIN_SET);
}

/* 启动 LCD 后端：先探测 SDRAM，再初始化 LTDC，最后显示测试图。 */
void display_lcd_init(void) {
  lcd_ready = false;
  (void)memset(&lcd_ui_last, 0, sizeof(lcd_ui_last));
  (void)memset(&lcd_input_last_estimate, 0, sizeof(lcd_input_last_estimate));
  lcd_input_last_estimate_ms = 0U;
  lcd_input_last_estimate_config_hz = 0U;
  lcd_input_estimate_miss_count = 0U;
  lcd_scene = LCD_SCENE_SPLASH;
  lcd_scene_started_ms = 0U;
  lcd_last_dynamic_refresh_ms = 0U;
  lcd_set_state("init");
  lcd_backlight_init();

  if (lcd_init_sdram() != HAL_OK) {
    lcd_set_state("sdram init failed");
    return;
  }

  if (!lcd_sdram_probe()) {
    lcd_set_state("sdram rw failed");
    return;
  }

  if (lcd_init_ltdc() != HAL_OK) {
    lcd_set_state("ltdc init failed");
    return;
  }

  lcd_fill_test_pattern();
  lcd_backlight_on();

  lcd_ready = true;
  lcd_set_state("ready test-pattern");
}

/* 查询 LCD 是否进入可绘制状态。 */
bool display_lcd_ready_impl(void) {
  return lcd_ready;
}

/* 返回当前 LCD 名称。 */
const char *display_lcd_name_impl(void) {
  return APP_LCD_NAME;
}

/* 返回当前 LCD 状态文本。 */
const char *display_lcd_status_impl(void) {
  return lcd_state;
}

/* 仅在 LCD 已 ready 时才处理 LTDC 中断。 */
void display_lcd_irq_handler(void) {
  if (lcd_ready) {
    HAL_LTDC_IRQHandler(&lcd_handle);
  }
}

/* LCD 当前不直接回显串口日志，保留接口便于后续扩展。 */
void display_lcd_write(const char *text) {
  (void)text;
}

/* 绘制 LCD 启动欢迎页。 */
void display_lcd_boot_banner(void) {
  const ui_ctrl_view_t *ui = ui_ctrl_view();

  if (!lcd_ready) {
    return;
  }

  if (ui != NULL) {
    lcd_scene = LCD_SCENE_SPLASH;
    lcd_scene_started_ms = HAL_GetTick();
    lcd_ui_last.valid = false;
    display_lcd_status(signal_gen_current(), signal_measure_latest());
  }
}

/* 目前 LCD 后端没有独立帮助页，保留空接口与 UART 后端对齐。 */
void display_lcd_help(void) {
}

/* 根据当前业务状态刷新 LCD 状态页。 */
void display_lcd_status(const signal_gen_config_t *config, const signal_measure_result_t *measurement) {
  const ui_ctrl_view_t *ui = ui_ctrl_view();

  if (!lcd_ready || config == NULL || ui == NULL) {
    return;
  }

  if (lcd_scene == LCD_SCENE_SPLASH &&
      lcd_scene_started_ms != 0U &&
      (HAL_GetTick() - lcd_scene_started_ms) >= LCD_SPLASH_DURATION_MS) {
    lcd_scene = LCD_SCENE_CONTROL;
    lcd_ui_last.valid = false;
    lcd_set_state("ready ui");
  }

  lcd_render_ui(ui, config, measurement);
}
