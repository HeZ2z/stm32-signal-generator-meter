#!/usr/bin/env sh
set -eu

cc -std=c11 -Wall -Wextra -Werror -DHOST_TEST -Iinclude \
  tests/test_ui_cmd.c src/ui_cmd.c \
  -o /tmp/test_ui_cmd

cc -std=c11 -Wall -Wextra -Werror -DHOST_TEST -Iinclude \
  tests/test_signal_gen_logic.c src/signal_gen_logic.c \
  -o /tmp/test_signal_gen_logic

/tmp/test_ui_cmd
/tmp/test_signal_gen_logic
