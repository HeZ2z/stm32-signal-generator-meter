#ifndef UI_CMD_H
#define UI_CMD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  UI_CMD_NONE = 0,
  UI_CMD_HELP,
  UI_CMD_STATUS,
  UI_CMD_SET_FREQ,
  UI_CMD_SET_DUTY,
  UI_CMD_INVALID,
} ui_cmd_kind_t;

typedef struct {
  ui_cmd_kind_t kind;
  uint32_t value;
} ui_cmd_t;

bool ui_cmd_parse(const char *line, ui_cmd_t *cmd);
bool ui_cmd_push_char(char *buffer,
                      size_t capacity,
                      uint8_t ch,
                      size_t *length,
                      bool *line_ready);

#endif
