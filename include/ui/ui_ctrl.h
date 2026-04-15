#ifndef UI_CTRL_H
#define UI_CTRL_H

/* 初始化串口控制状态。 */
void ui_ctrl_init(void);
/* 轮询串口输入并执行用户命令。 */
void ui_ctrl_poll(void);

#endif
