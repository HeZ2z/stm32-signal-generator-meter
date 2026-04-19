#ifndef UI_ACTIONS_H
#define UI_ACTIONS_H

#include <stdbool.h>

#include "ui/ui_cmd.h"
#include "ui/ui_ctrl.h"

/* 初始化动作模块（绑定 view 指针，由 ui_ctrl 在 init 时调用）。 */
void ui_actions_init(ui_ctrl_view_t *view);

/* 将配置值钳制到有效范围内。 */
signal_gen_dac_config_t clamp_config(signal_gen_dac_config_t config);

/* 将 pending_config 写入信号生成器。 */
void apply_pending_config(void);

/* 按增量修改 pending_config 并 apply。 */
void bump_config(int32_t freq_delta, int32_t duty_delta);

/* 处理 UART 命令（SET_FREQ / SET_DUTY / HELP / STATUS）。 */
void handle_ui_command(const ui_cmd_t *cmd);

#endif
