#include "touch/touch.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "board/board_config.h"
#include "main.h"

#define GT9XXX_CMD_WR 0x28U
#define GT9XXX_CMD_RD 0x29U

#define GT9XXX_CTRL_REG 0x8040U
#define GT9XXX_PID_REG 0x8140U
#define GT9XXX_GSTID_REG 0x814EU
#define GT9XXX_TP1_REG 0x8150U

static touch_runtime_t runtime;
static touch_event_t pending_event;
static bool event_pending;
static touch_point_t last_point;
static touch_point_t debounce_point;
static uint32_t last_move_ms;
static uint32_t press_candidate_ms;
static uint32_t release_candidate_ms;
static bool press_candidate_active;
static bool release_candidate_active;

/* 使用与 Apollo HAL 例程一致的 GT9xxx 时序，但保留当前项目的轻量接口。 */
static void touch_delay_cycle(void) {
  for (uint32_t i = 0; i < APP_TOUCH_I2C_DELAY_CYCLES; ++i) {
    __NOP();
  }
}

static void touch_set_status(const char *status) {
  (void)snprintf(runtime.status, sizeof(runtime.status), "%s", status);
}

static void touch_emit_event(touch_event_kind_t kind,
                             const touch_point_t *point,
                             bool pressed,
                             uint32_t now_ms) {
  pending_event.kind = kind;
  pending_event.point = *point;
  pending_event.pressed = pressed;
  pending_event.timestamp_ms = now_ms;
  event_pending = true;
  runtime.pressed = pressed;
  runtime.last_point = *point;
}

static uint16_t touch_abs_delta_u16(uint16_t a, uint16_t b) {
  return a > b ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

static bool touch_point_drifted(const touch_point_t *a, const touch_point_t *b, uint16_t limit_px) {
  return touch_abs_delta_u16(a->x, b->x) > limit_px ||
         touch_abs_delta_u16(a->y, b->y) > limit_px;
}

static void touch_gpio_clocks_enable(void) {
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();
}

static void touch_i2c_gpio_init(void) {
  GPIO_InitTypeDef gpio_init = {0};

  touch_gpio_clocks_enable();

  gpio_init.Pin = APP_TOUCH_I2C_SCL_PIN;
  gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
  gpio_init.Pull = GPIO_PULLUP;
  gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(APP_TOUCH_I2C_SCL_PORT, &gpio_init);

  gpio_init.Pin = APP_TOUCH_I2C_SDA_PIN;
  HAL_GPIO_Init(APP_TOUCH_I2C_SDA_PORT, &gpio_init);

  HAL_GPIO_WritePin(APP_TOUCH_I2C_SCL_PORT, APP_TOUCH_I2C_SCL_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(APP_TOUCH_I2C_SDA_PORT, APP_TOUCH_I2C_SDA_PIN, GPIO_PIN_SET);
}

static void touch_ctrl_gpio_init(void) {
  GPIO_InitTypeDef gpio_init = {0};

  touch_gpio_clocks_enable();

  gpio_init.Pin = APP_TOUCH_RST_PIN;
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Pull = GPIO_PULLUP;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(APP_TOUCH_RST_PORT, &gpio_init);

  gpio_init.Pin = APP_TOUCH_INT_PIN;
  gpio_init.Mode = GPIO_MODE_INPUT;
  gpio_init.Pull = GPIO_PULLUP;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(APP_TOUCH_INT_PORT, &gpio_init);
}

static void touch_i2c_scl(GPIO_PinState state) {
  HAL_GPIO_WritePin(APP_TOUCH_I2C_SCL_PORT, APP_TOUCH_I2C_SCL_PIN, state);
}

static void touch_i2c_sda(GPIO_PinState state) {
  HAL_GPIO_WritePin(APP_TOUCH_I2C_SDA_PORT, APP_TOUCH_I2C_SDA_PIN, state);
}

static GPIO_PinState touch_i2c_read_sda(void) {
  return HAL_GPIO_ReadPin(APP_TOUCH_I2C_SDA_PORT, APP_TOUCH_I2C_SDA_PIN);
}

static void touch_i2c_start(void) {
  touch_i2c_sda(GPIO_PIN_SET);
  touch_i2c_scl(GPIO_PIN_SET);
  touch_delay_cycle();
  touch_i2c_sda(GPIO_PIN_RESET);
  touch_delay_cycle();
  touch_i2c_scl(GPIO_PIN_RESET);
  touch_delay_cycle();
}

static void touch_i2c_stop(void) {
  touch_i2c_sda(GPIO_PIN_RESET);
  touch_delay_cycle();
  touch_i2c_scl(GPIO_PIN_SET);
  touch_delay_cycle();
  touch_i2c_sda(GPIO_PIN_SET);
  touch_delay_cycle();
}

static void touch_i2c_ack(void) {
  touch_i2c_sda(GPIO_PIN_RESET);
  touch_delay_cycle();
  touch_i2c_scl(GPIO_PIN_SET);
  touch_delay_cycle();
  touch_i2c_scl(GPIO_PIN_RESET);
  touch_delay_cycle();
  touch_i2c_sda(GPIO_PIN_SET);
}

static void touch_i2c_nack(void) {
  touch_i2c_sda(GPIO_PIN_SET);
  touch_delay_cycle();
  touch_i2c_scl(GPIO_PIN_SET);
  touch_delay_cycle();
  touch_i2c_scl(GPIO_PIN_RESET);
  touch_delay_cycle();
}

static bool touch_i2c_wait_ack(void) {
  uint32_t wait_count = 0U;

  touch_i2c_sda(GPIO_PIN_SET);
  touch_delay_cycle();
  touch_i2c_scl(GPIO_PIN_SET);
  touch_delay_cycle();

  while (touch_i2c_read_sda() == GPIO_PIN_SET) {
    if (++wait_count > 250U) {
      touch_i2c_stop();
      return false;
    }
    touch_delay_cycle();
  }

  touch_i2c_scl(GPIO_PIN_RESET);
  touch_delay_cycle();
  return true;
}

static void touch_i2c_send_byte(uint8_t data) {
  for (uint8_t bit = 0; bit < 8U; ++bit) {
    touch_i2c_sda((data & 0x80U) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
    touch_delay_cycle();
    touch_i2c_scl(GPIO_PIN_SET);
    touch_delay_cycle();
    touch_i2c_scl(GPIO_PIN_RESET);
    data <<= 1;
  }

  touch_i2c_sda(GPIO_PIN_SET);
}

static uint8_t touch_i2c_read_byte(bool ack) {
  uint8_t value = 0U;

  for (uint8_t bit = 0; bit < 8U; ++bit) {
    value <<= 1;
    touch_i2c_scl(GPIO_PIN_SET);
    touch_delay_cycle();
    if (touch_i2c_read_sda() == GPIO_PIN_SET) {
      value |= 0x01U;
    }
    touch_i2c_scl(GPIO_PIN_RESET);
    touch_delay_cycle();
  }

  if (ack) {
    touch_i2c_ack();
  } else {
    touch_i2c_nack();
  }

  return value;
}

static bool gt9xxx_write_reg(uint16_t reg, const uint8_t *data, uint8_t length) {
  touch_i2c_start();
  touch_i2c_send_byte(GT9XXX_CMD_WR);
  if (!touch_i2c_wait_ack()) {
    return false;
  }
  touch_i2c_send_byte((uint8_t)(reg >> 8));
  if (!touch_i2c_wait_ack()) {
    return false;
  }
  touch_i2c_send_byte((uint8_t)(reg & 0xFFU));
  if (!touch_i2c_wait_ack()) {
    return false;
  }

  for (uint8_t i = 0; i < length; ++i) {
    touch_i2c_send_byte(data[i]);
    if (!touch_i2c_wait_ack()) {
      return false;
    }
  }

  touch_i2c_stop();
  return true;
}

static bool gt9xxx_read_reg(uint16_t reg, uint8_t *data, uint8_t length) {
  touch_i2c_start();
  touch_i2c_send_byte(GT9XXX_CMD_WR);
  if (!touch_i2c_wait_ack()) {
    return false;
  }
  touch_i2c_send_byte((uint8_t)(reg >> 8));
  if (!touch_i2c_wait_ack()) {
    return false;
  }
  touch_i2c_send_byte((uint8_t)(reg & 0xFFU));
  if (!touch_i2c_wait_ack()) {
    return false;
  }

  touch_i2c_start();
  touch_i2c_send_byte(GT9XXX_CMD_RD);
  if (!touch_i2c_wait_ack()) {
    return false;
  }

  for (uint8_t i = 0; i < length; ++i) {
    data[i] = touch_i2c_read_byte(i < (length - 1U));
  }

  touch_i2c_stop();
  return true;
}

static bool gt9xxx_pid_supported(const char *pid) {
  return strcmp(pid, "911") == 0 ||
         strcmp(pid, "9147") == 0 ||
         strcmp(pid, "1158") == 0 ||
         strcmp(pid, "9271") == 0;
}

static bool touch_map_point(uint16_t raw_x, uint16_t raw_y, touch_point_t *point) {
  uint16_t mapped_x = raw_x;
  uint16_t mapped_y = raw_y;

  if (APP_TOUCH_SWAP_XY != 0U) {
    mapped_x = raw_y;
    mapped_y = raw_x;
  }
  if (APP_TOUCH_INVERT_X != 0U && mapped_x < APP_LCD_WIDTH) {
    mapped_x = (uint16_t)(APP_LCD_WIDTH - 1U - mapped_x);
  }
  if (APP_TOUCH_INVERT_Y != 0U && mapped_y < APP_LCD_HEIGHT) {
    mapped_y = (uint16_t)(APP_LCD_HEIGHT - 1U - mapped_y);
  }
  if (mapped_x >= APP_LCD_WIDTH || mapped_y >= APP_LCD_HEIGHT) {
    return false;
  }

  point->x = mapped_x;
  point->y = mapped_y;
  return true;
}

static void touch_clamp_point(uint16_t raw_x, uint16_t raw_y, touch_point_t *point) {
  uint16_t mapped_x = raw_x;
  uint16_t mapped_y = raw_y;

  if (APP_TOUCH_SWAP_XY != 0U) {
    mapped_x = raw_y;
    mapped_y = raw_x;
  }
  if (APP_TOUCH_INVERT_X != 0U && mapped_x < APP_LCD_WIDTH) {
    mapped_x = (uint16_t)(APP_LCD_WIDTH - 1U - mapped_x);
  }
  if (APP_TOUCH_INVERT_Y != 0U && mapped_y < APP_LCD_HEIGHT) {
    mapped_y = (uint16_t)(APP_LCD_HEIGHT - 1U - mapped_y);
  }

  if (mapped_x >= APP_LCD_WIDTH) {
    mapped_x = (uint16_t)(APP_LCD_WIDTH - 1U);
  }
  if (mapped_y >= APP_LCD_HEIGHT) {
    mapped_y = (uint16_t)(APP_LCD_HEIGHT - 1U);
  }

  point->x = mapped_x;
  point->y = mapped_y;
}

static bool gt9xxx_init_controller(void) {
  GPIO_InitTypeDef gpio_init = {0};
  uint8_t pid[5] = {0};
  uint8_t command = 0x02U;

  touch_ctrl_gpio_init();
  touch_i2c_gpio_init();

  HAL_GPIO_WritePin(APP_TOUCH_RST_PORT, APP_TOUCH_RST_PIN, GPIO_PIN_RESET);
  HAL_Delay(10U);
  HAL_GPIO_WritePin(APP_TOUCH_RST_PORT, APP_TOUCH_RST_PIN, GPIO_PIN_SET);
  HAL_Delay(10U);

  gpio_init.Pin = APP_TOUCH_INT_PIN;
  gpio_init.Mode = GPIO_MODE_INPUT;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(APP_TOUCH_INT_PORT, &gpio_init);

  HAL_Delay(100U);

  if (!gt9xxx_read_reg(GT9XXX_PID_REG, pid, 4U)) {
    touch_set_status("GT9XXX PID FAIL");
    return false;
  }

  pid[4] = '\0';
  if (!gt9xxx_pid_supported((char *)pid)) {
    touch_set_status("GT9XXX PID BAD");
    return false;
  }

  (void)snprintf(runtime.controller, sizeof(runtime.controller), "%s", (char *)pid);

  if (!gt9xxx_write_reg(GT9XXX_CTRL_REG, &command, 1U)) {
    touch_set_status("GT9XXX RST FAIL");
    return false;
  }
  HAL_Delay(10U);
  command = 0x00U;
  if (!gt9xxx_write_reg(GT9XXX_CTRL_REG, &command, 1U)) {
    touch_set_status("GT9XXX RUN FAIL");
    return false;
  }

  touch_set_status("GT9XXX READY");
  return true;
}

void touch_init(void) {
  (void)memset(&runtime, 0, sizeof(runtime));
  pending_event.kind = TOUCH_EVENT_NONE;
  event_pending = false;
  last_move_ms = 0U;
  last_point = (touch_point_t){0U, 0U};
  debounce_point = (touch_point_t){0U, 0U};
  press_candidate_ms = 0U;
  release_candidate_ms = 0U;
  press_candidate_active = false;
  release_candidate_active = false;
  runtime.state = TOUCH_STATE_INIT;
  touch_set_status("GT9XXX INIT");

  if (!gt9xxx_init_controller()) {
    runtime.state = TOUCH_STATE_ERROR;
    return;
  }

  runtime.state = TOUCH_STATE_READY;
}

void touch_poll(uint32_t now_ms) {
  uint8_t status = 0U;
  uint8_t point_data[4] = {0};
  uint8_t clear_status = 0U;
  touch_point_t point = {0};
  uint16_t raw_x;
  uint16_t raw_y;

  if (runtime.state != TOUCH_STATE_READY) {
    return;
  }

  if (!gt9xxx_read_reg(GT9XXX_GSTID_REG, &status, 1U)) {
    runtime.state = TOUCH_STATE_ERROR;
    touch_set_status("GT9XXX READ FAIL");
    return;
  }

  runtime.last_status = status;
  runtime.points = (uint8_t)(status & 0x0FU);

  /* 对齐 Apollo HAL 例程：只要 GT9xxx 标记“有新状态”，就先清掉状态位。
   * 否则像当前这种 st=0x80/points=0 的空状态会一直锁住，后续真实触点可能上报不进来。
   */
  if ((status & 0x80U) != 0U && runtime.points <= APP_TOUCH_MAX_POINTS) {
    (void)gt9xxx_write_reg(GT9XXX_GSTID_REG, &clear_status, 1U);
  }

  if ((status & 0x80U) != 0U && runtime.points > 0U && runtime.points <= APP_TOUCH_MAX_POINTS) {
    if (!gt9xxx_read_reg(GT9XXX_TP1_REG, point_data, 4U)) {
      runtime.state = TOUCH_STATE_ERROR;
      touch_set_status("GT9XXX TP FAIL");
      return;
    }

    raw_x = (uint16_t)(((uint16_t)point_data[1] << 8) | point_data[0]);
    raw_y = (uint16_t)(((uint16_t)point_data[3] << 8) | point_data[2]);
    runtime.raw_x = raw_x;
    runtime.raw_y = raw_y;

    if (!touch_map_point(raw_x, raw_y, &point)) {
      runtime.mapped_valid = false;
      touch_clamp_point(raw_x, raw_y, &point);
      touch_set_status("GT9XXX RAW OOB");
    } else {
      runtime.mapped_valid = true;
      touch_set_status("GT9XXX TOUCH");
    }

    if (!runtime.pressed) {
      release_candidate_active = false;
      release_candidate_ms = 0U;

      /* 触摸按下需要先稳定一个很短的窗口，避免轻微抖动被识别为多次点击。 */
      if (!press_candidate_active ||
          touch_point_drifted(&point, &debounce_point, APP_TOUCH_DEBOUNCE_DRIFT_PX)) {
        debounce_point = point;
        press_candidate_ms = now_ms;
        press_candidate_active = true;
        runtime.last_point = point;
        return;
      }

      runtime.last_point = point;
      if ((now_ms - press_candidate_ms) >= APP_TOUCH_PRESS_DEBOUNCE_MS) {
        touch_emit_event(TOUCH_EVENT_DOWN, &point, true, now_ms);
        last_point = point;
        last_move_ms = now_ms;
        press_candidate_active = false;
      }
      return;
    }

    press_candidate_active = false;
    press_candidate_ms = 0U;
    release_candidate_active = false;
    release_candidate_ms = 0U;
    runtime.last_point = point;
    if ((uint32_t)touch_abs_delta_u16(point.x, last_point.x) >= APP_TOUCH_MOVE_DELTA_PX ||
        (uint32_t)touch_abs_delta_u16(point.y, last_point.y) >= APP_TOUCH_MOVE_DELTA_PX ||
        (now_ms - last_move_ms) >= APP_TOUCH_MOVE_INTERVAL_MS) {
      touch_emit_event(TOUCH_EVENT_MOVE, &point, true, now_ms);
      last_point = point;
      last_move_ms = now_ms;
    }
    return;
  }

  if (runtime.pressed) {
    /* 释放同样做短暂确认，避免控制器在手指离屏边缘反复上下跳变。 */
    if (!release_candidate_active) {
      release_candidate_active = true;
      release_candidate_ms = now_ms;
      return;
    }

    if ((now_ms - release_candidate_ms) >= APP_TOUCH_RELEASE_DEBOUNCE_MS) {
      touch_emit_event(TOUCH_EVENT_UP, &last_point, false, now_ms);
      touch_set_status("GT9XXX READY");
      runtime.mapped_valid = true;
      release_candidate_active = false;
      release_candidate_ms = 0U;
    }
    return;
  }

  press_candidate_active = false;
  press_candidate_ms = 0U;
}

bool touch_ready(void) {
  return runtime.state == TOUCH_STATE_READY;
}

bool touch_has_event(void) {
  return event_pending;
}

bool touch_pop_event(touch_event_t *event) {
  if (!event_pending || event == NULL) {
    return false;
  }

  *event = pending_event;
  pending_event.kind = TOUCH_EVENT_NONE;
  event_pending = false;
  return true;
}

bool touch_is_calibrated(void) {
  return runtime.state == TOUCH_STATE_READY;
}

bool touch_is_calibrating(void) {
  return false;
}

bool touch_supported(void) {
  return runtime.state == TOUCH_STATE_READY || runtime.state == TOUCH_STATE_ERROR;
}

void touch_request_calibration(void) {
  touch_set_status("GT9XXX NO CAL");
}

const touch_runtime_t *touch_runtime(void) {
  return &runtime;
}
