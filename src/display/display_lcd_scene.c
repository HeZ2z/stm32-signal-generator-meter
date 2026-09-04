#include "display/display_lcd_scene.h"
#include "display/display_lcd_scope.h"
#include "display/display_lcd_xy.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "board/board_config.h"
#include "display/display_backend.h"
#include "display/lcd_font.h"
#include "display/lcd_prim.h"
#include "signal_capture/signal_capture_adc.h"
#include "signal_gen/signal_gen_dac.h"

#define LCD_DYNAMIC_REFRESH_MS APP_SCOPE_LCD_REFRESH_MS
#define LCD_BUTTON_COUNT 6

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
  signal_gen_dac_config_t pending_config;
  char title[32];
  char footer[64];
  char set_line[40];
  char meas_line[40];
  char err_line[40];
  char touch_line[40];
  char hint_line[40];
} lcd_ui_snapshot_t;

/* 场景状态（由 facade 持有但 scene.c 需要写入）。 */
lcd_scene_t lcd_scene = LCD_SCENE_SPLASH;
uint32_t lcd_scene_started_ms;
static uint32_t lcd_last_dynamic_refresh_ms;

static lcd_ui_snapshot_t lcd_ui_last;

#if APP_LCD_PROFILE_ENABLE
typedef enum {
  LCD_PROFILE_SCENE = 0,
  LCD_PROFILE_SCOPE_PLOT,
  LCD_PROFILE_SCOPE_INFO,
  LCD_PROFILE_XY_PLOT,
  LCD_PROFILE_XY_INFO,
  LCD_PROFILE_FOOTER,
  LCD_PROFILE_COUNT,
} lcd_profile_slot_t;

static struct {
  bool dwt_ready;
  uint32_t last_log_ms;
  uint32_t max_cycles[LCD_PROFILE_COUNT];
} lcd_profile_state;

static void lcd_profile_init(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  lcd_profile_state.dwt_ready = true;
  lcd_profile_state.last_log_ms = HAL_GetTick();
  (void)memset(lcd_profile_state.max_cycles, 0, sizeof(lcd_profile_state.max_cycles));
}

static uint32_t lcd_profile_begin(void) {
  return lcd_profile_state.dwt_ready ? DWT->CYCCNT : 0U;
}

static void lcd_profile_record(lcd_profile_slot_t slot, uint32_t start_cycles) {
  uint32_t elapsed;

  if (!lcd_profile_state.dwt_ready) {
    return;
  }

  elapsed = DWT->CYCCNT - start_cycles;
  if (elapsed > lcd_profile_state.max_cycles[slot]) {
    lcd_profile_state.max_cycles[slot] = elapsed;
  }
}

static uint32_t lcd_profile_cycles_to_us(uint32_t cycles, uint32_t hclk_hz) {
  return (uint32_t)(((uint64_t)cycles * 1000U) / (hclk_hz / 1000U));
}

static void lcd_profile_maybe_log(uint32_t now_ms) {
  char buffer[192];
  uint32_t hclk_hz;

  if (!lcd_profile_state.dwt_ready ||
      (now_ms - lcd_profile_state.last_log_ms) < 1000U) {
    return;
  }

  hclk_hz = HAL_RCC_GetHCLKFreq();
  (void)snprintf(
      buffer, sizeof(buffer),
      "LCD scene_us=%lu scopePlot_us=%lu scopeInfo_us=%lu xyPlot_us=%lu xyInfo_us=%lu footer_us=%lu\r\n",
      (unsigned long)lcd_profile_cycles_to_us(lcd_profile_state.max_cycles[LCD_PROFILE_SCENE], hclk_hz),
      (unsigned long)lcd_profile_cycles_to_us(lcd_profile_state.max_cycles[LCD_PROFILE_SCOPE_PLOT], hclk_hz),
      (unsigned long)lcd_profile_cycles_to_us(lcd_profile_state.max_cycles[LCD_PROFILE_SCOPE_INFO], hclk_hz),
      (unsigned long)lcd_profile_cycles_to_us(lcd_profile_state.max_cycles[LCD_PROFILE_XY_PLOT], hclk_hz),
      (unsigned long)lcd_profile_cycles_to_us(lcd_profile_state.max_cycles[LCD_PROFILE_XY_INFO], hclk_hz),
      (unsigned long)lcd_profile_cycles_to_us(lcd_profile_state.max_cycles[LCD_PROFILE_FOOTER], hclk_hz));
  display_uart_write(buffer);
  lcd_profile_state.last_log_ms = now_ms;
  (void)memset(lcd_profile_state.max_cycles, 0, sizeof(lcd_profile_state.max_cycles));
}
#endif

static bool lcd_config_changed(const signal_gen_dac_config_t *previous,
                               const signal_gen_dac_config_t *current) {
  return previous->frequency_hz != current->frequency_hz ||
         previous->waveform != current->waveform ||
         previous->ch_b_phase_offset_rad != current->ch_b_phase_offset_rad ||
         previous->ch_b_frequency_ratio != current->ch_b_frequency_ratio;
}

static void lcd_format_touch_line(const ui_ctrl_view_t *ui, char *buffer, size_t size) {
  if (!ui->touch_ready) {
    (void)snprintf(buffer, size, "%s", "TOUCH INIT...");
  } else if (ui->last_touch.x != 0U || ui->last_touch.y != 0U) {
    (void)snprintf(buffer, size, "TOUCH X=%u Y=%u",
                   ui->last_touch.x, ui->last_touch.y);
  } else {
    (void)snprintf(buffer, size, "%s", "TOUCH READY");
  }
}

void lcd_draw_string(uint16_t x, uint16_t y, const char *text,
                     uint16_t fg, uint16_t bg, uint8_t scale) {
  uint16_t advance = (uint16_t)(6U * scale);
  while (*text != '\0') {
    lcd_draw_char(x, y, *text, fg, bg, scale);
    x = (uint16_t)(x + advance);
    ++text;
  }
}

static void lcd_format_status(lcd_status_view_t *view,
                               const signal_measure_result_t *measurement) {
  scope_capture_frame_t frame = {0};
  const signal_gen_dac_status_t *dac = signal_gen_dac_current();
  uint32_t now = HAL_GetTick();
  (void)measurement;

  signal_capture_adc_read_frame(&frame, now);

  (void)snprintf(view->header, sizeof(view->header), "REAL-TIME WAVEFORM");
  if (dac->active) {
    (void)snprintf(view->set_line, sizeof(view->set_line),
                   "DAC %s F=%luHZ  VIEW=%s",
                   signal_gen_dac_waveform_short_name(dac->waveform),
                   (unsigned long)dac->frequency_hz,
                   lcd_scene == LCD_SCENE_XY ? "XY" : "YT");
  } else {
    (void)snprintf(view->set_line, sizeof(view->set_line),
                   "OUTPUT STOPPED");
  }
  if (frame.ch_a.valid || frame.ch_b.valid) {
    (void)snprintf(view->meas_line, sizeof(view->meas_line),
                   "ADC CH-A=%s CH-B=%s",
                   frame.ch_a.valid ? "LIVE" : "MISS",
                   frame.ch_b.valid ? "LIVE" : "MISS");
    (void)snprintf(view->err_line, sizeof(view->err_line),
                   "SCOPE READY  DAC -> ADC LOOPBACK");
    return;
  }

  (void)snprintf(view->meas_line, sizeof(view->meas_line), "ADC NO-SIGNAL CH-A/CH-B");
  (void)snprintf(view->err_line, sizeof(view->err_line), "CHECK PA4->PA0 / PA5->PA6");
}

void display_lcd_scene_init(void) {
  lcd_scene = LCD_SCENE_SPLASH;
  lcd_scene_started_ms = 0U;
  lcd_last_dynamic_refresh_ms = 0U;
  (void)memset(&lcd_ui_last, 0, sizeof(lcd_ui_last));
#if APP_LCD_PROFILE_ENABLE
  lcd_profile_init();
#endif
}

void display_lcd_scene_set(lcd_scene_t scene) {
  lcd_scene = scene;
  lcd_scene_started_ms = HAL_GetTick();
  lcd_ui_last.valid = false;
}

void lcd_draw_splash(void) {
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

  lcd_draw_string(56, 72, "HEZ2Z/STM32-SIGNAL-GENERATOR", text, panel, 2);
  lcd_draw_string(68, 112, "DUAL DAC + DUAL ADC + TOUCH", accent, panel, 2);
  lcd_draw_string(126, 152, "SEE GITHUB REPO", accent2, panel, 2);
  lcd_draw_string(140, 222, "APOLLO F429 RGBLCD DEMO", text, bg, 1);
}

void lcd_draw_control_static(const ui_ctrl_view_t *ui) {
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

void lcd_draw_footer(const ui_ctrl_view_t *ui) {
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

void lcd_draw_more_overlay(void) {
  uint16_t panel = lcd_rgb565(10, 24, 52);
  uint16_t border = lcd_rgb565(110, 196, 255);
  uint16_t accent = lcd_rgb565(255, 190, 84);
  uint16_t text = lcd_rgb565(238, 246, 255);
  uint16_t link = lcd_rgb565(120, 224, 255);

  lcd_draw_card(LCD_MORE_X, LCD_MORE_Y, LCD_MORE_WIDTH, LCD_MORE_HEIGHT,
                border, panel, accent);

  lcd_draw_string((uint16_t)(LCD_MORE_X + 16U),
                  (uint16_t)(LCD_MORE_Y + 16U),
                  "HEZ2Z/STM32-SIGNAL-GENERATOR", text, panel, 1);
  lcd_draw_string((uint16_t)(LCD_MORE_X + 34U),
                  (uint16_t)(LCD_MORE_Y + 34U),
                  "DUAL DAC + DUAL ADC DEMO", accent, panel, 1);
  lcd_draw_hline((uint16_t)(LCD_MORE_X + 16U),
                  (uint16_t)(LCD_MORE_Y + 54U), 304U,
                  lcd_rgb565(34, 72, 118));

  lcd_draw_string((uint16_t)(LCD_MORE_X + 16U),
                  (uint16_t)(LCD_MORE_Y + 66U),
                  "REPO HEZ2Z/STM32-SIGNAL-GENERATOR-METER", link, panel, 1);
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
                  "SCOPE PA4/PA5 DAC -> PA0/PA6 ADC", text, panel, 1);
  lcd_draw_string((uint16_t)(LCD_MORE_X + 16U),
                  (uint16_t)(LCD_MORE_Y + 140U),
                  "TAP MORE OR BLANK AREA TO CLOSE", accent, panel, 1);
}

void lcd_restore_more_overlay(const ui_ctrl_view_t *ui,
                              const signal_measure_result_t *measurement) {
  uint16_t bg = lcd_rgb565(7, 16, 34);
  uint16_t border = lcd_rgb565(54, 122, 178);
  uint16_t panel = lcd_rgb565(14, 28, 56);
  scope_capture_frame_t frame = {0};
  uint32_t now = HAL_GetTick();
  (void)measurement;

  lcd_fill_rect(LCD_MORE_X, LCD_MORE_Y, LCD_MORE_WIDTH, LCD_MORE_HEIGHT, bg);
  lcd_draw_hline(18, 46, 444, border);
  lcd_draw_card(LCD_SCOPE_X, LCD_SCOPE_Y, LCD_SCOPE_WIDTH, LCD_SCOPE_HEIGHT,
                border, panel, lcd_rgb565(85, 210, 255));
  lcd_draw_scope_static_plot_frame();
  display_lcd_scope_init();
  display_lcd_xy_init();

  for (size_t i = 0; i < LCD_BUTTON_COUNT; ++i) {
    lcd_draw_button(lcd_buttons[i].x,
                    lcd_buttons[i].y,
                    lcd_buttons[i].width,
                    lcd_buttons[i].height,
                    lcd_button_label((int)lcd_buttons[i].id),
                    ui->highlight == lcd_buttons[i].id);
  }

  signal_capture_adc_read_frame(&frame, now);
  if (lcd_scene == LCD_SCENE_XY) {
    lcd_xy_plot_refresh(&frame, true);
    lcd_xy_info_refresh(signal_gen_dac_current());
  } else {
    lcd_scope_plot_refresh(&frame);
    lcd_scope_info_refresh(signal_gen_dac_current(), &frame);
  }
  lcd_draw_footer(ui);
}

void lcd_render_ui(const ui_ctrl_view_t *ui,
                   const signal_measure_result_t *measurement) {
  lcd_status_view_t status_view = {0};
  bool scene_changed;
  bool highlight_changed;
  bool footer_changed;
  bool touch_changed;
  bool config_changed;
  bool set_changed;
  bool meas_changed;
  bool err_changed;
  bool hint_changed;
  bool overlay_open;
  bool overlay_opened;
  bool overlay_closed;
  bool plot_tick_due;
  bool plot_dirty;
  bool info_dirty;
  bool footer_dirty;
  bool overlay_restore_dirty;
  bool frame_ready = false;
  scope_capture_frame_t frame = {0};
  uint32_t now = HAL_GetTick();
#if APP_LCD_PROFILE_ENABLE
  uint32_t scene_start = lcd_profile_begin();
#endif

  lcd_format_status(&status_view, measurement);
  lcd_format_touch_line(ui, status_view.touch_line, sizeof(status_view.touch_line));
  (void)snprintf(status_view.hint_line, sizeof(status_view.hint_line),
                 "UART HELP/STATUS/FREQ");

  scene_changed = !lcd_ui_last.valid || lcd_ui_last.scene != lcd_scene;
  highlight_changed = !scene_changed && lcd_ui_last.highlight != ui->highlight;
  footer_changed = scene_changed || strcmp(lcd_ui_last.footer, ui->footer) != 0;
  touch_changed = scene_changed ||
                  strcmp(lcd_ui_last.touch_line, status_view.touch_line) != 0;
  config_changed = scene_changed ||
                   lcd_config_changed(&lcd_ui_last.pending_config,
                                      &ui->pending_config);
  set_changed = scene_changed || strcmp(lcd_ui_last.set_line, status_view.set_line) != 0;
  meas_changed = scene_changed || strcmp(lcd_ui_last.meas_line, status_view.meas_line) != 0;
  err_changed = scene_changed || strcmp(lcd_ui_last.err_line, status_view.err_line) != 0;
  hint_changed = scene_changed || strcmp(lcd_ui_last.hint_line, status_view.hint_line) != 0;
  overlay_open = ui->more_open;
  overlay_opened = !scene_changed && !lcd_ui_last.more_open && overlay_open;
  overlay_closed = !scene_changed && lcd_ui_last.more_open && !overlay_open;
  overlay_restore_dirty = overlay_closed;
  plot_tick_due = (lcd_last_dynamic_refresh_ms == 0U) ||
                  ((now - lcd_last_dynamic_refresh_ms) >= LCD_DYNAMIC_REFRESH_MS);
  plot_dirty = !overlay_open &&
               (scene_changed || overlay_restore_dirty || plot_tick_due || config_changed);
  info_dirty = !overlay_open &&
               (scene_changed || overlay_restore_dirty || plot_tick_due ||
                config_changed || set_changed || meas_changed || err_changed);
  footer_dirty = scene_changed || overlay_restore_dirty || footer_changed ||
                 touch_changed || hint_changed;

  if (scene_changed && lcd_scene == LCD_SCENE_SPLASH) {
    lcd_draw_splash();
  } else if (lcd_scene == LCD_SCENE_CONTROL || lcd_scene == LCD_SCENE_XY) {
    if (scene_changed) {
      display_lcd_scope_init();
      display_lcd_xy_init();
      lcd_draw_control_static(ui);
      if (!overlay_open) {
        signal_capture_adc_read_frame(&frame, now);
        frame_ready = true;
        if (lcd_scene == LCD_SCENE_XY) {
#if APP_LCD_PROFILE_ENABLE
          {
            uint32_t slot_start = lcd_profile_begin();
            lcd_xy_plot_refresh(&frame, true);
            lcd_profile_record(LCD_PROFILE_XY_PLOT, slot_start);
          }
          {
            uint32_t slot_start = lcd_profile_begin();
            lcd_xy_info_refresh(signal_gen_dac_current());
            lcd_profile_record(LCD_PROFILE_XY_INFO, slot_start);
          }
#else
          lcd_xy_plot_refresh(&frame, true);
          lcd_xy_info_refresh(signal_gen_dac_current());
#endif
        } else {
#if APP_LCD_PROFILE_ENABLE
          {
            uint32_t slot_start = lcd_profile_begin();
            lcd_scope_plot_refresh(&frame);
            lcd_profile_record(LCD_PROFILE_SCOPE_PLOT, slot_start);
          }
          {
            uint32_t slot_start = lcd_profile_begin();
            lcd_scope_info_refresh(signal_gen_dac_current(), &frame);
            lcd_profile_record(LCD_PROFILE_SCOPE_INFO, slot_start);
          }
#else
          lcd_scope_plot_refresh(&frame);
          lcd_scope_info_refresh(signal_gen_dac_current(), &frame);
#endif
        }
      }
      if (footer_dirty && !overlay_open) {
#if APP_LCD_PROFILE_ENABLE
        {
          uint32_t slot_start = lcd_profile_begin();
          lcd_draw_footer(ui);
          lcd_profile_record(LCD_PROFILE_FOOTER, slot_start);
        }
#else
        lcd_draw_footer(ui);
#endif
      }
      if (overlay_open) {
        lcd_draw_more_overlay();
      }
      lcd_last_dynamic_refresh_ms = now;
    } else {
      if (overlay_opened) {
        lcd_redraw_button(UI_HIGHLIGHT_HELP, true);
        lcd_draw_more_overlay();
        lcd_last_dynamic_refresh_ms = now;
      } else if (overlay_closed) {
        lcd_restore_more_overlay(ui, measurement);
        lcd_redraw_button(UI_HIGHLIGHT_HELP, false);
        lcd_last_dynamic_refresh_ms = now;
      } else if (highlight_changed && !overlay_open) {
        lcd_redraw_button((int)lcd_ui_last.highlight, false);
        lcd_redraw_button((int)ui->highlight, true);
      }

      if (!overlay_open && !overlay_closed && (plot_dirty || info_dirty)) {
        signal_capture_adc_read_frame(&frame, now);
        frame_ready = true;
      }

      if (!overlay_open && !overlay_closed && plot_dirty) {
        if (lcd_scene == LCD_SCENE_XY) {
#if APP_LCD_PROFILE_ENABLE
          {
            uint32_t slot_start = lcd_profile_begin();
            lcd_xy_plot_refresh(&frame, config_changed);
            lcd_profile_record(LCD_PROFILE_XY_PLOT, slot_start);
          }
#else
          lcd_xy_plot_refresh(&frame, config_changed);
#endif
        } else {
#if APP_LCD_PROFILE_ENABLE
          {
            uint32_t slot_start = lcd_profile_begin();
            lcd_scope_plot_refresh(&frame);
            lcd_profile_record(LCD_PROFILE_SCOPE_PLOT, slot_start);
          }
#else
          lcd_scope_plot_refresh(&frame);
#endif
        }
      }

      if (!overlay_open && !overlay_closed && info_dirty) {
        if (!frame_ready) {
          signal_capture_adc_read_frame(&frame, now);
        }
        if (lcd_scene == LCD_SCENE_XY) {
#if APP_LCD_PROFILE_ENABLE
          {
            uint32_t slot_start = lcd_profile_begin();
            lcd_xy_info_refresh(signal_gen_dac_current());
            lcd_profile_record(LCD_PROFILE_XY_INFO, slot_start);
          }
#else
          lcd_xy_info_refresh(signal_gen_dac_current());
#endif
        } else {
#if APP_LCD_PROFILE_ENABLE
          {
            uint32_t slot_start = lcd_profile_begin();
            lcd_scope_info_refresh(signal_gen_dac_current(), &frame);
            lcd_profile_record(LCD_PROFILE_SCOPE_INFO, slot_start);
          }
#else
          lcd_scope_info_refresh(signal_gen_dac_current(), &frame);
#endif
        }
      }

      if (!overlay_open && !overlay_closed && footer_dirty) {
#if APP_LCD_PROFILE_ENABLE
        {
          uint32_t slot_start = lcd_profile_begin();
          lcd_draw_footer(ui);
          lcd_profile_record(LCD_PROFILE_FOOTER, slot_start);
        }
 #else
        lcd_draw_footer(ui);
 #endif
      }
      if (!overlay_open && (plot_dirty || info_dirty)) {
        lcd_last_dynamic_refresh_ms = now;
      }
    }
  }

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

#if APP_LCD_PROFILE_ENABLE
  lcd_profile_record(LCD_PROFILE_SCENE, scene_start);
  lcd_profile_maybe_log(now);
#endif
}
