#include "display/display_lcd_hw.h"

#include <stdio.h>
#include <string.h>

#include "board/board_config.h"
#include "display/lcd_prim.h"

/* ------------------------------------------------------------------ */
/*  底层句柄与全局状态                                               */
/* ------------------------------------------------------------------ */

LTDC_HandleTypeDef lcd_handle;
SDRAM_HandleTypeDef sdram_handle;
bool lcd_hw_ready;
char lcd_hw_state[64] = APP_LCD_STATUS;

/* ------------------------------------------------------------------ */
/*  工具函数                                                         */
/* ------------------------------------------------------------------ */

static void lcd_set_state(const char *state) {
  (void)snprintf(lcd_hw_state, sizeof(lcd_hw_state), "%s", state);
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

/* ------------------------------------------------------------------ */
/*  背光与测试图案                                                   */
/* ------------------------------------------------------------------ */

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
/*  公共初始化入口                                                   */
/* ------------------------------------------------------------------ */

void display_lcd_hw_init(void) {
  lcd_hw_ready = false;
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

  lcd_hw_ready = true;
  lcd_set_state("ready test-pattern");
}

const char *display_lcd_hw_state(void) {
  return lcd_hw_state;
}

void display_lcd_hw_irq_handler(void) {
  if (lcd_hw_ready) {
    HAL_LTDC_IRQHandler(&lcd_handle);
  }
}
