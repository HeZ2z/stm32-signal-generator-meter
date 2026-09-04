#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "ui/ui_cmd.h"

/* 该文件覆盖命令解析和命令缓冲状态机。 */
static int failures = 0;

/* 简易断言助手：失败时打印用例意图并累计失败数。 */
static void expect(bool condition, const char *message) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s\n", message);
    failures++;
  }
}

/* help/status/freq/duty 是当前项目对外承诺的最小命令集。 */
static void test_parse_help(void) {
  ui_cmd_t cmd;
  expect(ui_cmd_parse("help", &cmd), "help should parse");
  expect(cmd.kind == UI_CMD_HELP, "help kind");
}

static void test_parse_status(void) {
  ui_cmd_t cmd;
  expect(ui_cmd_parse("status", &cmd), "status should parse");
  expect(cmd.kind == UI_CMD_STATUS, "status kind");
}

static void test_parse_freq(void) {
  ui_cmd_t cmd;
  expect(ui_cmd_parse("freq 2000", &cmd), "freq should parse");
  expect(cmd.kind == UI_CMD_SET_FREQ, "freq kind");
  expect(cmd.value == 2000U, "freq value");
}

static void test_parse_duty(void) {
  ui_cmd_t cmd;
  expect(ui_cmd_parse("duty 30", &cmd), "duty should parse");
  expect(cmd.kind == UI_CMD_SET_DUTY, "duty kind");
  expect(cmd.value == 30U, "duty value");
}

static void test_parse_phase(void) {
  ui_cmd_t cmd;
  expect(ui_cmd_parse("phase 628", &cmd), "phase should parse");
  expect(cmd.kind == UI_CMD_SET_PHASE, "phase kind");
  expect(cmd.value == 628U, "phase value");
}

static void test_parse_ratio(void) {
  ui_cmd_t cmd;
  expect(ui_cmd_parse("ratio 2", &cmd), "ratio should parse");
  expect(cmd.kind == UI_CMD_SET_RATIO, "ratio kind");
  expect(cmd.value == 2U, "ratio value");
}

static void test_parse_invalid(void) {
  ui_cmd_t cmd;
  expect(!ui_cmd_parse("hello", &cmd), "invalid command rejected");
  expect(cmd.kind == UI_CMD_INVALID, "invalid kind");
}

static void test_parse_leading_space(void) {
  ui_cmd_t cmd;
  expect(ui_cmd_parse("   freq 2000", &cmd), "leading spaces should parse");
  expect(cmd.kind == UI_CMD_SET_FREQ, "leading spaces freq kind");
  expect(cmd.value == 2000U, "leading spaces freq value");
}

/* 覆盖串口逐字节接收、回车成行、溢出和退格场景。 */
static void test_push_char(void) {
  char buffer[8] = {0};
  size_t length = 0;
  bool line_ready = false;

  expect(ui_cmd_push_char(buffer, sizeof(buffer), 'a', &length, &line_ready), "push a");
  expect(length == 1U, "length after a");
  expect(!line_ready, "no line after a");
  expect(ui_cmd_push_char(buffer, sizeof(buffer), '\r', &length, &line_ready), "push cr");
  expect(line_ready, "line ready after cr");
  expect(buffer[0] == 'a' && buffer[1] == '\0', "buffer contains a");
  expect(length == 0U, "length reset after line");
}

static void test_push_char_too_long(void) {
  char buffer[4] = {0};
  size_t length = 0;
  bool line_ready = false;

  expect(ui_cmd_push_char(buffer, sizeof(buffer), 'a', &length, &line_ready), "push 1");
  expect(ui_cmd_push_char(buffer, sizeof(buffer), 'b', &length, &line_ready), "push 2");
  expect(ui_cmd_push_char(buffer, sizeof(buffer), 'c', &length, &line_ready), "push 3");
  expect(!ui_cmd_push_char(buffer, sizeof(buffer), 'd', &length, &line_ready), "overflow rejected");
  expect(length == 0U, "length reset on overflow");
}

static void test_push_char_backspace(void) {
  char buffer[8] = {0};
  size_t length = 0;
  bool line_ready = false;

  expect(ui_cmd_push_char(buffer, sizeof(buffer), 'a', &length, &line_ready), "push a for backspace");
  expect(ui_cmd_push_char(buffer, sizeof(buffer), 'b', &length, &line_ready), "push b for backspace");
  expect(ui_cmd_push_char(buffer, sizeof(buffer), '\b', &length, &line_ready), "backspace accepted");
  expect(length == 1U, "length decremented after backspace");
  expect(buffer[0] == 'a' && buffer[1] == '\0', "buffer updated after backspace");
}

int main(void) {
  test_parse_help();
  test_parse_status();
  test_parse_freq();
  test_parse_duty();
  test_parse_phase();
  test_parse_ratio();
  test_parse_invalid();
  test_parse_leading_space();
  test_push_char();
  test_push_char_too_long();
  test_push_char_backspace();

  if (failures != 0) {
    return 1;
  }

  puts("PASS: test_ui_cmd");
  return 0;
}
