#include "display/display_backend.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "board/board_config.h"

#define SDRAM_MODEREG_BURST_LENGTH_1          0x0000U
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   0x0000U
#define SDRAM_MODEREG_CAS_LATENCY_3           0x0030U
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD 0x0000U
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE  0x0200U

typedef struct {
  char header[40];
  char set_line[40];
  char meas_line[40];
  char err_line[40];
} lcd_status_view_t;

static LTDC_HandleTypeDef lcd_handle;
static SDRAM_HandleTypeDef sdram_handle;
static bool lcd_ready;
static bool lcd_frame_ready;
static char lcd_state[64] = APP_LCD_STATUS;
static lcd_status_view_t lcd_last_view;

static uint16_t *const framebuffer = (uint16_t *)APP_LCD_FRAMEBUFFER_ADDRESS;

static void lcd_set_state(const char *state) {
  (void)snprintf(lcd_state, sizeof(lcd_state), "%s", state);
}

static uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
  return (uint16_t)(((red & 0xF8U) << 8) | ((green & 0xFCU) << 3) | (blue >> 3));
}

static void lcd_plot(uint16_t x, uint16_t y, uint16_t color) {
  if (x >= APP_LCD_WIDTH || y >= APP_LCD_HEIGHT) {
    return;
  }

  framebuffer[(y * APP_LCD_WIDTH) + x] = color;
}

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

static const uint8_t *glyph_for_char(char ch) {
  static const uint8_t blank[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
  static const uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
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

static void lcd_draw_string(uint16_t x, uint16_t y, const char *text, uint16_t fg, uint16_t bg, uint8_t scale) {
  const uint16_t advance = (uint16_t)(6U * scale);
  while (*text != '\0') {
    lcd_draw_char(x, y, *text, fg, bg, scale);
    x = (uint16_t)(x + advance);
    ++text;
  }
}

static void lcd_clear(void) {
  lcd_fill_rect(0, 0, APP_LCD_WIDTH, APP_LCD_HEIGHT, rgb565(8, 18, 42));
}

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

static void lcd_draw_frame(void) {
  uint16_t accent = rgb565(70, 180, 255);
  uint16_t panel = rgb565(16, 32, 68);
  uint16_t line = rgb565(28, 54, 98);

  lcd_clear();
  lcd_fill_rect(12, 12, 456, 34, accent);
  lcd_fill_rect(12, 54, 456, 186, panel);
  lcd_fill_rect(12, 100, 456, 2, line);
  lcd_fill_rect(12, 146, 456, 2, line);
  lcd_fill_rect(12, 192, 456, 2, line);
}

static void lcd_clear_line(uint16_t x, uint16_t y, uint16_t width, uint8_t scale) {
  uint16_t panel_bg = rgb565(16, 32, 68);
  uint16_t height = (uint16_t)(7U * scale);
  lcd_fill_rect(x, y, width, height, panel_bg);
}

static void lcd_draw_status_line(uint16_t x,
                                 uint16_t y,
                                 const char *text,
                                 uint16_t fg,
                                 uint16_t bg,
                                 uint8_t scale) {
  lcd_clear_line(x, y, 432U, scale);
  lcd_draw_string(x, y, text, fg, bg, scale);
}

static void lcd_draw_status_view(const lcd_status_view_t *view) {
  uint16_t title_fg = rgb565(0, 22, 50);
  uint16_t body_fg = rgb565(230, 242, 255);
  uint16_t panel_bg = rgb565(16, 32, 68);
  uint16_t title_bg = rgb565(70, 180, 255);

  if (!lcd_frame_ready) {
    lcd_draw_frame();
    lcd_draw_string(22, 20, view->header, title_fg, title_bg, 2);
    lcd_frame_ready = true;
    lcd_last_view.header[0] = '\0';
    lcd_last_view.set_line[0] = '\0';
    lcd_last_view.meas_line[0] = '\0';
    lcd_last_view.err_line[0] = '\0';
  }

  if (strcmp(lcd_last_view.header, view->header) != 0) {
    lcd_fill_rect(22, 20, 420, 14, title_bg);
    lcd_draw_string(22, 20, view->header, title_fg, title_bg, 2);
    (void)snprintf(lcd_last_view.header, sizeof(lcd_last_view.header), "%s", view->header);
  }

  if (strcmp(lcd_last_view.set_line, view->set_line) != 0) {
    lcd_draw_status_line(24, 66, view->set_line, body_fg, panel_bg, 2);
    (void)snprintf(lcd_last_view.set_line, sizeof(lcd_last_view.set_line), "%s", view->set_line);
  }

  if (strcmp(lcd_last_view.meas_line, view->meas_line) != 0) {
    lcd_draw_status_line(24, 112, view->meas_line, body_fg, panel_bg, 2);
    (void)snprintf(lcd_last_view.meas_line, sizeof(lcd_last_view.meas_line), "%s", view->meas_line);
  }

  if (strcmp(lcd_last_view.err_line, view->err_line) != 0) {
    lcd_draw_status_line(24, 158, view->err_line, body_fg, panel_bg, 2);
    (void)snprintf(lcd_last_view.err_line, sizeof(lcd_last_view.err_line), "%s", view->err_line);
  }
}

static void lcd_format_status(lcd_status_view_t *view,
                              const signal_gen_config_t *config,
                              const signal_measure_result_t *measurement) {
  int32_t freq_error = 0;
  int32_t period_error = 0;
  int32_t duty_error = 0;

  (void)snprintf(view->header, sizeof(view->header), "APOLLO F429 RGBLCD");
  (void)snprintf(view->set_line,
                 sizeof(view->set_line),
                 "SET F=%" PRIu32 "HZ D=%u%%",
                 config->frequency_hz,
                 config->duty_percent);

  if (measurement != NULL && measurement->valid) {
    freq_error = (int32_t)measurement->frequency_hz - (int32_t)config->frequency_hz;
    period_error = (int32_t)measurement->period_us -
                   (int32_t)(1000000UL / config->frequency_hz);
    duty_error = (int32_t)measurement->duty_percent - (int32_t)config->duty_percent;

    (void)snprintf(view->meas_line,
                   sizeof(view->meas_line),
                   "MEAS F=%" PRIu32 "HZ P=%" PRIu32 "US D=%u%%",
                   measurement->frequency_hz,
                   measurement->period_us,
                   measurement->duty_percent);
    (void)snprintf(view->err_line,
                   sizeof(view->err_line),
                   "ERR DF=%" PRId32 " DT=%" PRId32 " DD=%" PRId32,
                   freq_error,
                   period_error,
                   duty_error);
    return;
  }

  (void)snprintf(view->meas_line, sizeof(view->meas_line), "MEAS NO-SIGNAL");
  (void)snprintf(view->err_line, sizeof(view->err_line), "CHECK PB6-PA7 LOOP");
}

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

static void lcd_backlight_on(void) {
  HAL_GPIO_WritePin(APP_LCD_BACKLIGHT_PORT, APP_LCD_BACKLIGHT_PIN, GPIO_PIN_SET);
}

void display_lcd_init(void) {
  lcd_ready = false;
  lcd_frame_ready = false;
  (void)memset(&lcd_last_view, 0, sizeof(lcd_last_view));
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

bool display_lcd_ready_impl(void) {
  return lcd_ready;
}

const char *display_lcd_name_impl(void) {
  return APP_LCD_NAME;
}

const char *display_lcd_status_impl(void) {
  return lcd_state;
}

void display_lcd_irq_handler(void) {
  if (lcd_ready) {
    HAL_LTDC_IRQHandler(&lcd_handle);
  }
}

void display_lcd_write(const char *text) {
  (void)text;
}

void display_lcd_boot_banner(void) {
  lcd_status_view_t view = {0};

  if (!lcd_ready) {
    return;
  }

  (void)snprintf(view.header, sizeof(view.header), "APOLLO F429 RGBLCD");
  (void)snprintf(view.set_line, sizeof(view.set_line), "LCD READY 480X272 RGB");
  (void)snprintf(view.meas_line, sizeof(view.meas_line), "PWM PB6  MEAS PA7");
  (void)snprintf(view.err_line, sizeof(view.err_line), "UART DEBUG STILL ACTIVE");
  lcd_draw_status_view(&view);
}

void display_lcd_help(void) {
}

void display_lcd_status(const signal_gen_config_t *config, const signal_measure_result_t *measurement) {
  lcd_status_view_t view = {0};

  if (!lcd_ready || config == NULL) {
    return;
  }

  lcd_format_status(&view, config, measurement);
  lcd_draw_status_view(&view);
}
