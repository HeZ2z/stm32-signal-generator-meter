#ifndef UI_CMD_H
#define UI_CMD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 串口命令解析后得到的命令类型。 */
typedef enum {
  UI_CMD_NONE = 0,
  UI_CMD_HELP,
  UI_CMD_STATUS,
  UI_CMD_SET_FREQ,
  UI_CMD_SET_DUTY,
  UI_CMD_INVALID,
} ui_cmd_kind_t;

/* 一条命令的抽象表示；value 仅对带参数命令有效。 */
typedef struct {
  ui_cmd_kind_t kind;
  uint32_t value;
} ui_cmd_t;

/* 将一整行命令文本解析成结构化命令。 */
bool ui_cmd_parse(const char *line, ui_cmd_t *cmd);
/* 增量接收串口字符，并在遇到行结束时交付完整命令。 */
bool ui_cmd_push_char(char *buffer,
                      size_t capacity,
                      uint8_t ch,
                      size_t *length,
                      bool *line_ready);

#endif
