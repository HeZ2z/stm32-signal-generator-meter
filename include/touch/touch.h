#ifndef TOUCH_H
#define TOUCH_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint16_t x;
  uint16_t y;
} touch_point_t;

typedef enum {
  TOUCH_EVENT_NONE = 0,
  TOUCH_EVENT_DOWN,
  TOUCH_EVENT_MOVE,
  TOUCH_EVENT_UP,
} touch_event_kind_t;

typedef struct {
  touch_event_kind_t kind;
  touch_point_t point;
  bool pressed;
  uint32_t timestamp_ms;
} touch_event_t;

typedef enum {
  TOUCH_STATE_INIT = 0,
  TOUCH_STATE_READY,
  TOUCH_STATE_ERROR,
} touch_state_t;

typedef struct {
  touch_state_t state;
  bool pressed;
  bool mapped_valid;
  touch_point_t last_point;
  uint16_t raw_x;
  uint16_t raw_y;
  uint8_t last_status;
  uint8_t points;
  char status[32];
  char controller[8];
} touch_runtime_t;

void touch_init(void);
void touch_poll(uint32_t now_ms);
bool touch_ready(void);
bool touch_has_event(void);
bool touch_pop_event(touch_event_t *event);
bool touch_is_calibrated(void);
bool touch_is_calibrating(void);
bool touch_supported(void);
void touch_request_calibration(void);
const touch_runtime_t *touch_runtime(void);

#endif
