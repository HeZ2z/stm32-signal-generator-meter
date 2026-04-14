# Source Layout

本目录用于存放项目源码实现。

当前推荐按模块拆分：

- `app/`
- `signal_gen/`
- `signal_measure/`
- `ui_ctrl/`
- `display/`

实现顺序建议：

1. 先建立最小启动与 `UART` 输出
2. 再补 `PWM` 输出
3. 再补输入捕获与参数计算
4. 最后再做参数调整和展示优化
