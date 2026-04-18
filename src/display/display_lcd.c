#include "display/display_backend.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "board/board_config.h"
#include "display/lcd_font.h"
#include "display/lcd_prim.h"
#include "scope/scope_render_logic.h"
#include "signal_capture/signal_capture_adc.h"
#include "signal_gen/signal_gen.h"
#include "signal_measure/signal_measure.h"
#include "touch/touch.h"
#include "ui/ui_ctrl.h"

/* lcd_buttons[] 的定义在 lcd_prim.c，extern 声明已在 lcd_prim.h 中。 */

/* 前向声明：lcd_draw_scope_static_plot_frame 定义在后面。 */
static void lcd_draw_scope_static_plot_frame(void);

/* 按钮数量常量（避免对 extern incomplete 数组取 sizeof）。 */
#define LCD_BUTTON_COUNT 6

/* ------------------------------------------------------------------ */
/*  LCD 场景状态（由 lcd_prim.h 提供坐标宏，lcd_font.h 提供字符绘制） */
/* ------------------------------------------------------------------ */

#define LCD_SPLASH_DURATION_MS 1800U
#define LCD_DYNAMIC_REFRESH_MS APP_SCOPE_LCD_REFRESH_MS

typedef enum {
  LCD_SCENE_SPLASH = 0,
  LCD_SCENE_CONTROL,
} lcd_scene_t;

/* LCD 状态页在屏幕上显示的四行文本。 */
typedef struct {
  char header[40];
  char set_line[40];
  char meas_line[40];
  char err_line[40];
  char touch_line[40];
  char hint_line[40];
} lcd_status_view_t;

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

/* ------------------------------------------------------------------ */
/*  底层句柄与全局状态                                               */
/* ------------------------------------------------------------------ */

static LTDC_HandleTypeDef lcd_handle;
static SDRAM_HandleTypeDef sdram_handle;
static bool lcd_ready;
static char lcd_state[64] = APP_LCD_STATUS;
static lcd_scene_t lcd_scene = LCD_SCENE_SPLASH;
static uint32_t lcd_scene_started_ms;
static uint32_t lcd_last_dynamic_refresh_ms;

static lcd_ui_snapshot_t lcd_ui_last;

/* ------------------------------------------------------------------ */
/*  示波器相关状态（lcd_scope_* 函数共享）                           */
/* ------------------------------------------------------------------ */

static scope_render_trace_t lcd_scope_last_trace;
static bool lcd_scope_last_trace_valid;
static bool lcd_scope_last_no_signal;
static scope_square_wave_estimate_t lcd_input_last_estimate;
static uint32_t lcd_input_last_estimate_ms;
static uint32_t lcd_input_last_estimate_config_hz;
static uint8_t lcd_input_estimate_miss_count;

/* ------------------------------------------------------------------ */
/*  工具函数（不得拆分的小型辅助）                                   */
/* ------------------------------------------------------------------ */

static void lcd_set_state(const char *state) {
  (void)snprintf(lcd_state, sizeof(lcd_state), "%s", state);
}

static void lcd_draw_string(uint16_t x, uint16_t y, const char *text,
                            uint16_t fg, uint16_t bg, uint8_t scale) {
  uint16_t advance = (uint16_t)(6U * scale);
  while (*text != '\0') {
    lcd_draw_char(x, y, *text, fg, bg, scale);
    x = (uint16_t)(x + advance);
    ++text;
  }
}

/* ------------------------------------------------------------------ */
/*  SDRAM 初始化                                                     */
/* ------------------------------------------------------------------ */

#define SDRAM_MODEREG_BURST_LENGTH_1          0x0000U
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   0x0000U
#define SDRAM_MODEREG_CAS_LATENCY_3           0x0030U
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD 0x0000U
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE  0x0200U

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

static bool lcd_sdram_probe(void) {
  static const uint16_t pattern[] = {
      0xF800U, 0x07E0U, 0x001FU, 0xFFFFU,
      0x0000U, 0xA145U, 0x5AA5U, 0x1357U,
  };
  volatile uint16_t *framebuffer = (volatile uint16_t *)APP_LCD_FRAMEBUFFER_ADDRESS;

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

/* ------------------------------------------------------------------ */
/*  LTDC 初始化                                                      */
/* ------------------------------------------------------------------ */

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

static void lcd_fill_test_pattern(void) {
  uint16_t half_w = APP_LCD_WIDTH / 2U;
  uint16_t half_h = APP_LCD_HEIGHT / 2U;

  lcd_fill_rect(0, 0, half_w, half_h, lcd_rgb565(255, 0, 0));
  lcd_fill_rect(half_w, 0, (uint16_t)(APP_LCD_WIDTH - half_w), half_h, lcd_rgb565(0, 255, 0));
  lcd_fill_rect(0, half_h, half_w, (uint16_t)(APP_LCD_HEIGHT - half_h), lcd_rgb565(0, 0, 255));
  lcd_fill_rect(half_w, half_h, (uint16_t)(APP_LCD_WIDTH - half_w),
                (uint16_t)(APP_LCD_HEIGHT - half_h), lcd_rgb565(255, 255, 0));

  lcd_fill_rect(220, 116, 40, 40, lcd_rgb565(255, 255, 255));
  lcd_fill_rect(228, 124, 24, 24, lcd_rgb565(0, 0, 0));
}

/* ------------------------------------------------------------------ */
/*  场景绘制                                                         */
/* ------------------------------------------------------------------ */

static void lcd_draw_splash(void) {
  uint16_t bg = lcd_rgb565(6, 16, 36);
  uint16_t accent = lcd_rgb565(87, 212, 255);
  uint16_t accent2 = lcd_rgb565(255, 180, 92);
  uint16_t panel = lcd_rgb565(12, 30, 62);
  uint16_t text = lcd_rgb565(236, 246, 255);
  uint16_t shadow = lcd_rgb565(18, 55, 92);

  lcd_clear(bg);
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
  uint16_t bg = lcd_rgb565(7, 16, 34);
  uint16_t chrome = lcd_rgb565(20, 48, 86);
  uint16_t panel = lcd_rgb565(14, 28, 56);
  uint16_t border = lcd_rgb565(54, 122, 178);

  lcd_clear(bg);
  lcd_fill_rect(0, 0, APP_LCD_WIDTH, APP_LCD_HEIGHT, bg);
  lcd_fill_rect(0, 0, APP_LCD_WIDTH, 12U, chrome);
  lcd_fill_rect(0, (uint16_t)(APP_LCD_HEIGHT - 10U), APP_LCD_WIDTH, 10U, chrome);
  lcd_draw_hline(18, 46, 444, border);
  lcd_draw_card(LCD_SCOPE_X, LCD_SCOPE_Y, LCD_SCOPE_WIDTH, LCD_SCOPE_HEIGHT,
                border, panel, lcd_rgb565(85, 210, 255));
  lcd_draw_scope_static_plot_frame();
  for (size_t i = 0; i < LCD_BUTTON_COUNT; ++i) {
    lcd_draw_button(lcd_buttons[i].x,
                    lcd_buttons[i].y,
                    lcd_buttons[i].width,
                    lcd_buttons[i].height,
                    lcd_button_label((int)lcd_buttons[i].id),
                    ui->highlight == lcd_buttons[i].id);
  }
}

static void lcd_draw_footer(const ui_ctrl_view_t *ui) {
  uint16_t warn = lcd_rgb565(255, 208, 116);
  uint16_t text = lcd_rgb565(236, 246, 255);
  uint16_t footer_bg = lcd_rgb565(10, 22, 46);
  char touch_line[40];

  if (!ui->touch_ready) {
    (void)snprintf(touch_line, sizeof(touch_line), "TOUCH INIT...");
  } else if (ui->last_touch.x != 0U || ui->last_touch.y != 0U) {
    (void)snprintf(touch_line, sizeof(touch_line), "TOUCH X=%u Y=%u",
                   ui->last_touch.x, ui->last_touch.y);
  } else {
    (void)snprintf(touch_line, sizeof(touch_line), "TOUCH READY");
  }

  lcd_fill_rect(18, 260, 444, 10, footer_bg);
  lcd_draw_string(22, 260, touch_line, text, footer_bg, 1);
  lcd_draw_string(250, 260, ui->footer, warn, footer_bg, 1);
}

/* ------------------------------------------------------------------ */
/*  示波器绘制                                                       */
/* ------------------------------------------------------------------ */

static uint32_t lcd_scope_adc_sample_rate_hz(void) {
  uint32_t pclk2_hz = HAL_RCC_GetPCLK2Freq();
  return pclk2_hz / 4U / 96U;
}

static void lcd_format_status(lcd_status_view_t *view,
                              const signal_gen_config_t *config,
                              const signal_measure_result_t *measurement) {
  const scope_capture_snapshot_t *snapshot = signal_capture_adc_latest();
  (void)measurement;

  (void)snprintf(view->header, sizeof(view->header), "REAL-TIME WAVEFORM");
  (void)snprintf(view->set_line, sizeof(view->set_line),
                 "SET F=%" PRIu32 "HZ D=%u%%",
                 config->frequency_hz, config->duty_percent);

  if (snapshot != NULL && snapshot->valid) {
    (void)snprintf(view->meas_line, sizeof(view->meas_line),
                   "ADC MIN=%u MAX=%u AVG=%u",
                   snapshot->min_raw, snapshot->max_raw, snapshot->mean_raw);
    (void)snprintf(view->err_line, sizeof(view->err_line),
                   "SCOPE READY  PB6 -> PA0");
    return;
  }

  (void)snprintf(view->meas_line, sizeof(view->meas_line), "ADC NO-SIGNAL");
  (void)snprintf(view->err_line, sizeof(view->err_line), "CHECK PB6-PA0 LOOPBACK");
}

static void lcd_draw_info_cards(const signal_gen_config_t *config,
                                const signal_measure_result_t *measurement,
                                const scope_capture_snapshot_t *snapshot) {
  uint16_t panel = lcd_rgb565(7, 16, 34);
  uint16_t border = lcd_rgb565(54, 122, 178);
  uint16_t input_accent = lcd_rgb565(112, 232, 162);
  uint16_t output_accent = lcd_rgb565(255, 182, 86);
  uint16_t text = lcd_rgb565(236, 246, 255);
  uint16_t warn = lcd_rgb565(255, 208, 116);
  char line[40];
  scope_square_wave_estimate_t estimate = {0};
  uint32_t min_frequency_hz = 0U;
  uint32_t max_frequency_hz = 0U;
  uint32_t adc_sample_rate_hz = lcd_scope_adc_sample_rate_hz();
  uint32_t now_ms = HAL_GetTick();
  bool duty_frequency_window_valid = scope_square_wave_frequency_window(
      APP_SCOPE_SAMPLE_COUNT, adc_sample_rate_hz,
      &min_frequency_hz, &max_frequency_hz);
  bool within_duty_window = duty_frequency_window_valid &&
                            config->frequency_hz >= min_frequency_hz &&
                            config->frequency_hz <= max_frequency_hz;
  bool square_signal_present = snapshot != NULL && snapshot->valid &&
                               scope_square_wave_signal_present(
                                   snapshot->samples, snapshot->sample_count,
                                   APP_SCOPE_SIGNAL_FLAT_THRESHOLD,
                                   APP_SCOPE_SIGNAL_MIN_SPAN,
                                   APP_SCOPE_SQUARE_LOW_LEVEL_MAX,
                                   APP_SCOPE_SQUARE_HIGH_LEVEL_MIN);
  bool can_hold_last_estimate =
      lcd_input_last_estimate.valid &&
      lcd_input_last_estimate_config_hz == config->frequency_hz &&
      (now_ms - lcd_input_last_estimate_ms) <= APP_SCOPE_ESTIMATE_HOLD_MS;
  bool can_tolerate_estimate_miss =
      lcd_input_last_estimate.valid &&
      lcd_input_last_estimate_config_hz == config->frequency_hz &&
      lcd_input_estimate_miss_count < APP_SCOPE_ESTIMATE_MISS_MAX;
  (void)measurement;

  if (square_signal_present && within_duty_window) {
    (void)scope_estimate_square_wave(snapshot->samples,
                                     snapshot->sample_count,
                                     APP_SCOPE_SIGNAL_FLAT_THRESHOLD,
                                     adc_sample_rate_hz, &estimate);
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
    (void)snprintf(line, sizeof(line), "F=%luHZ D=%u%%",
                   (unsigned long)estimate.frequency_hz, estimate.duty_percent);
    lcd_draw_string(26, 30, line, text, panel, 1);
  } else if (can_hold_last_estimate || can_tolerate_estimate_miss) {
    (void)snprintf(line, sizeof(line), "F=%luHZ D=%u%%",
                   (unsigned long)lcd_input_last_estimate.frequency_hz,
                   lcd_input_last_estimate.duty_percent);
    lcd_draw_string(26, 30, line, text, panel, 1);
  } else if (square_signal_present) {
    if (within_duty_window) {
      lcd_draw_string(26, 30, "F=ADC LIVE D=--", warn, panel, 1);
    } else {
      (void)snprintf(line, sizeof(line), "F=%luHZ D=--",
                     (unsigned long)config->frequency_hz);
      lcd_draw_string(26, 30, line, warn, panel, 1);
    }
  } else {
    lcd_draw_string(26, 30, "NO INPUT", warn, panel, 1);
  }

  lcd_draw_string(190, 20, "OUTPUT", text, panel, 1);
  lcd_draw_string(238, 20, "PB6 TIM4", output_accent, panel, 1);
  (void)snprintf(line, sizeof(line), "F=%luHZ D=%u%%",
                 (unsigned long)config->frequency_hz, config->duty_percent);
  lcd_draw_string(190, 30, line, text, panel, 1);
}

static void lcd_draw_scope_static_plot_frame(void) {
  uint16_t bg = lcd_rgb565(10, 20, 42);
  uint16_t grid = lcd_rgb565(32, 66, 108);

  lcd_fill_rect(LCD_SCOPE_PLOT_X, LCD_SCOPE_PLOT_Y,
                LCD_SCOPE_PLOT_WIDTH, LCD_SCOPE_PLOT_HEIGHT, bg);
  lcd_draw_box(LCD_SCOPE_PLOT_X, LCD_SCOPE_PLOT_Y,
               LCD_SCOPE_PLOT_WIDTH, LCD_SCOPE_PLOT_HEIGHT, grid, bg);
  lcd_draw_hline(LCD_SCOPE_PLOT_X,
                  (uint16_t)(LCD_SCOPE_PLOT_Y + (LCD_SCOPE_PLOT_HEIGHT / 2U)),
                  LCD_SCOPE_PLOT_WIDTH, grid);
  for (uint16_t x = (uint16_t)(LCD_SCOPE_PLOT_X + 53U);
       x < (LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH); x = (uint16_t)(x + 53U)) {
    lcd_draw_vline(x, LCD_SCOPE_PLOT_Y, LCD_SCOPE_PLOT_HEIGHT, grid);
  }
}

static void lcd_restore_scope_columns(uint16_t x_begin, uint16_t x_end) {
  uint16_t bg = lcd_rgb565(10, 20, 42);
  uint16_t grid = lcd_rgb565(32, 66, 108);
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
                 width, grid);
  for (uint16_t x = start; x <= end; ++x) {
    if (x == LCD_SCOPE_PLOT_X ||
        x == (uint16_t)(LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH - 1U) ||
        ((uint16_t)(x - LCD_SCOPE_PLOT_X) % 53U) == 0U) {
      lcd_draw_vline(x, LCD_SCOPE_PLOT_Y, LCD_SCOPE_PLOT_HEIGHT, grid);
    }
  }
}

static void lcd_draw_scope_trace(const scope_capture_snapshot_t *snapshot) {
  scope_render_trace_t trace = {0};
  uint16_t trace_color = lcd_rgb565(112, 232, 162);
  uint16_t warn = lcd_rgb565(255, 208, 116);
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
                    "NO SIGNAL ON PA0", warn,
                    lcd_rgb565(10, 20, 42), 2);
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
                        (((uint32_t)first_changed * (LCD_SCOPE_PLOT_WIDTH - 1U)) /
                         (trace.point_count - 1U)));
      uint16_t x_end = (uint16_t)(LCD_SCOPE_PLOT_X +
                      (((uint32_t)last_changed * (LCD_SCOPE_PLOT_WIDTH - 1U)) /
                       (trace.point_count - 1U)));
      x_begin = x_begin > (LCD_SCOPE_PLOT_X + 2U) ?
                (uint16_t)(x_begin - 2U) : LCD_SCOPE_PLOT_X;
      x_end = (uint16_t)(x_end + 2U);
      lcd_restore_scope_columns(x_begin, x_end);
    }
  }

  for (uint16_t i = 1; i < trace.point_count; ++i) {
    uint16_t x0 = (uint16_t)(LCD_SCOPE_PLOT_X +
                  (((uint32_t)(i - 1U) * (LCD_SCOPE_PLOT_WIDTH - 1U)) /
                   (trace.point_count - 1U)));
    uint16_t x1 = (uint16_t)(LCD_SCOPE_PLOT_X +
                  (((uint32_t)i * (LCD_SCOPE_PLOT_WIDTH - 1U)) /
                   (trace.point_count - 1U)));
    uint16_t y0 = (uint16_t)(LCD_SCOPE_PLOT_Y + trace.y_points[i - 1U]);
    uint16_t y1 = (uint16_t)(LCD_SCOPE_PLOT_Y + trace.y_points[i]);
    lcd_draw_line(x0, y0, x1, y1, trace_color);
  }

  (void)snprintf(label, sizeof(label), "N=%u", trace.point_count);
  lcd_draw_string((uint16_t)(LCD_SCOPE_PLOT_X + LCD_SCOPE_PLOT_WIDTH - 44U),
                  (uint16_t)(LCD_SCOPE_PLOT_Y + 4U),
                  label, lcd_rgb565(236, 246, 255),
                  lcd_rgb565(10, 20, 42), 1);
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

/* ------------------------------------------------------------------ */
/*  MORE 浮层                                                        */
/* ------------------------------------------------------------------ */

static void lcd_draw_more_overlay(void) {
  uint16_t panel = lcd_rgb565(10, 24, 52);
  uint16_t border = lcd_rgb565(110, 196, 255);
  uint16_t accent = lcd_rgb565(255, 190, 84);
  uint16_t text = lcd_rgb565(238, 246, 255);
  uint16_t link = lcd_rgb565(120, 224, 255);

  lcd_draw_card(LCD_MORE_X, LCD_MORE_Y, LCD_MORE_WIDTH, LCD_MORE_HEIGHT,
                border, panel, accent);

  lcd_draw_string((uint16_t)(LCD_MORE_X + 16U),
                  (uint16_t)(LCD_MORE_Y + 16U),
                  "HEZZZ/STM32-SIGNAL-GENERATOR", text, panel, 1);
  lcd_draw_string((uint16_t)(LCD_MORE_X + 34U),
                  (uint16_t)(LCD_MORE_Y + 34U),
                  "PWM + MEASURE + TOUCH DEMO", accent, panel, 1);
  lcd_draw_hline((uint16_t)(LCD_MORE_X + 16U),
                  (uint16_t)(LCD_MORE_Y + 54U), 304U,
                  lcd_rgb565(34, 72, 118));

  lcd_draw_string((uint16_t)(LCD_MORE_X + 16U),
                  (uint16_t)(LCD_MORE_Y + 66U),
                  "REPO HEZ2Z/STM32-SIGNAL-GENERATOR", link, panel, 1);
  lcd_draw_string((uint16_t)(LCD_MORE_X + 16U),
                  (uint16_t)(LCD_MORE_Y + 80U), "METER", link, panel, 1);
  lcd_draw_string((uint16_t)(LCD_MORE_X + 16U),
                  (uint16_t)(LCD_MORE_Y + 98U),
                  "BOARD APOLLO STM32F429IGT6", text, panel, 1);
  lcd_draw_string((uint16_t)(LCD_MORE_X + 16U),
                  (uint16_t)(LCD_MORE_Y + 112U),
                  "LCD ALIENTEK 4342 RGBLCD + GT9147", text, panel, 1);
  lcd_draw_string((uint16_t)(LCD_MORE_X + 16U),
                  (uint16_t)(LCD_MORE_Y + 126U),
                  "SCOPE PB6 TIM4 CH1 -> PA0 ADC1 IN0", text, panel, 1);
  lcd_draw_string((uint16_t)(LCD_MORE_X + 16U),
                  (uint16_t)(LCD_MORE_Y + 140U),
                  "TAP MORE OR BLANK AREA TO CLOSE", accent, panel, 1);
}

static void lcd_restore_more_overlay(const ui_ctrl_view_t *ui,
                                     const lcd_status_view_t *status_view,
                                     const signal_measure_result_t *measurement) {
  uint16_t bg = lcd_rgb565(7, 16, 34);
  uint16_t border = lcd_rgb565(54, 122, 178);
  uint16_t panel = lcd_rgb565(14, 28, 56);
  (void)status_view;

  lcd_fill_rect(LCD_MORE_X, LCD_MORE_Y, LCD_MORE_WIDTH, LCD_MORE_HEIGHT, bg);
  lcd_draw_hline(18, 46, 444, border);
  lcd_draw_card(LCD_SCOPE_X, LCD_SCOPE_Y, LCD_SCOPE_WIDTH, LCD_SCOPE_HEIGHT,
                border, panel, lcd_rgb565(85, 210, 255));
  lcd_draw_scope_static_plot_frame();

  for (size_t i = 0; i < LCD_BUTTON_COUNT; ++i) {
    lcd_draw_button(lcd_buttons[i].x,
                    lcd_buttons[i].y,
                    lcd_buttons[i].width,
                    lcd_buttons[i].height,
                    lcd_button_label((int)lcd_buttons[i].id),
                    ui->highlight == lcd_buttons[i].id);
  }

  lcd_draw_info_cards(&ui->pending_config, measurement, signal_capture_adc_latest());
  lcd_draw_scope_trace(signal_capture_adc_latest());
}

/* ------------------------------------------------------------------ */
/*  场景渲染编排（dirty 追踪 + 增量刷新）                           */
/* ------------------------------------------------------------------ */

static void lcd_render_ui(const ui_ctrl_view_t *ui,
                          const signal_gen_config_t *config,
                          const signal_measure_result_t *measurement) {
  lcd_status_view_t status_view = {0};

  lcd_format_status(&status_view, config, measurement);
  if (!ui->touch_ready) {
    (void)snprintf(status_view.touch_line, sizeof(status_view.touch_line),
                   "%s", "TOUCH INIT...");
  } else if (ui->last_touch.x != 0U || ui->last_touch.y != 0U) {
    (void)snprintf(status_view.touch_line, sizeof(status_view.touch_line),
                   "TOUCH X=%u Y=%u", ui->last_touch.x, ui->last_touch.y);
  } else {
    (void)snprintf(status_view.touch_line, sizeof(status_view.touch_line),
                   "%s", "TOUCH READY");
  }
  (void)snprintf(status_view.hint_line, sizeof(status_view.hint_line),
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
  bool needs_dynamic = title_changed || more_changed || footer_changed || touch_changed ||
                       freq_changed || duty_changed || set_changed || meas_changed ||
                       err_changed || hint_changed;

  if (scene_changed && lcd_scene == LCD_SCENE_SPLASH) {
    lcd_draw_splash();
  } else if (lcd_scene == LCD_SCENE_CONTROL) {
    uint32_t now = HAL_GetTick();
    bool overlay_open = ui->more_open;
    bool force_refresh = scene_changed || more_changed || highlight_changed ||
                         freq_changed || duty_changed || footer_changed || touch_changed;
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
        lcd_redraw_button((int)lcd_ui_last.highlight, false);
        lcd_redraw_button((int)ui->highlight, true);
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

/* ------------------------------------------------------------------ */
/*  Facade 接口（display_backend.h 定义的对外承诺）                   */
/* ------------------------------------------------------------------ */

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

void display_lcd_help(void) {
}

void display_lcd_status(const signal_gen_config_t *config,
                        const signal_measure_result_t *measurement) {
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
