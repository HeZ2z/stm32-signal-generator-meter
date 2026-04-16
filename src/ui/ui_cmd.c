#include "ui/ui_cmd.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* 解析一整行命令文本，仅支持当前项目约定的最小命令集。 */
bool ui_cmd_parse(const char *line, ui_cmd_t *cmd) {
  unsigned long value = 0;
  const char *cursor;

  cmd->kind = UI_CMD_INVALID;
  cmd->value = 0U;

  if (line == NULL) {
    cmd->kind = UI_CMD_NONE;
    return false;
  }

  cursor = line;
  while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
    ++cursor;
  }

  if (*cursor == '\0') {
    cmd->kind = UI_CMD_NONE;
    return false;
  }

  /* 纯文本命令优先做精确匹配，带参数命令再走 sscanf。 */
  if (strcmp(cursor, "help") == 0) {
    cmd->kind = UI_CMD_HELP;
    return true;
  }

  if (strcmp(cursor, "status") == 0) {
    cmd->kind = UI_CMD_STATUS;
    return true;
  }

  if (sscanf(cursor, "freq %lu", &value) == 1) {
    cmd->kind = UI_CMD_SET_FREQ;
    cmd->value = (uint32_t)value;
    return true;
  }

  if (sscanf(cursor, "duty %lu", &value) == 1) {
    cmd->kind = UI_CMD_SET_DUTY;
    cmd->value = (uint32_t)value;
    return true;
  }

  return false;
}

/* 将串口收到的单个字符累积为一行命令。 */
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

  if (ch == '\b' || ch == 0x7FU) {
    if (*length > 0U) {
      *length -= 1U;
      buffer[*length] = '\0';
    }
    return true;
  }

  if (*length < (capacity - 1U)) {
    buffer[*length] = (char)ch;
    *length += 1U;
    buffer[*length] = '\0';
    return true;
  }

  *length = 0U;
  return false;
}
