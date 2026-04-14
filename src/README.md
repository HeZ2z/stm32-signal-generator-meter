# Source Layout

本目录用于存放项目源码实现。

当前推荐按模块拆分：

- `app/`
- `signal_gen/`
- `ui_ctrl/`
- `display/`

实现顺序建议：

1. 先建立最小启动与 `UART` 输出
2. 再补 `PWM` 输出
3. 最后再做参数调整和展示优化

当前已提供：

- `main.c` / `app.c`：启动与主循环
- `display.c`：串口输出
- `signal_gen.c`：`TIM4_CH1` PWM 输出
- `ui_ctrl.c`：串口命令解析
