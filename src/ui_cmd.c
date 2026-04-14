#include "ui_cmd.h"

#include <stdio.h>
#include <string.h>

bool ui_cmd_parse(const char *line, ui_cmd_t *cmd) {
  unsigned long value = 0;

  cmd->kind = UI_CMD_INVALID;
  cmd->value = 0U;

  if (line == NULL || line[0] == '\0') {
    cmd->kind = UI_CMD_NONE;
    return false;
  }

  if (strcmp(line, "help") == 0) {
    cmd->kind = UI_CMD_HELP;
    return true;
  }

  if (strcmp(line, "status") == 0) {
    cmd->kind = UI_CMD_STATUS;
    return true;
  }

  if (sscanf(line, "freq %lu", &value) == 1) {
    cmd->kind = UI_CMD_SET_FREQ;
    cmd->value = (uint32_t)value;
    return true;
  }

  if (sscanf(line, "duty %lu", &value) == 1) {
    cmd->kind = UI_CMD_SET_DUTY;
    cmd->value = (uint32_t)value;
    return true;
  }

  return false;
}

bool ui_cmd_push_char(char *buffer,
                      size_t capacity,
                      uint8_t ch,
                      size_t *length,
                      bool *line_ready) {
  *line_ready = false;

  if (ch == '\r' || ch == '\n') {
    if (*length > 0U) {
      buffer[*length] = '\0';
      *line_ready = true;
      *length = 0U;
    }
    return true;
  }

  if (*length < (capacity - 1U)) {
    buffer[*length] = (char)ch;
    *length += 1U;
    return true;
  }

  *length = 0U;
  return false;
}
