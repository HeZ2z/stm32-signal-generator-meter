# Source Layout

本目录用于存放项目源码实现。

当前源码已按功能模块拆分：

- `app/`：应用调度和主循环协调
- `signal_gen/`：PWM 输出与参数换算
- `signal_measure/`：输入测量与结果换算
- `ui/`：串口命令解析与控制
- `display/`：显示输出实现，当前为 UART，并预留 RGBLCD 后端
- `board/`：板级中断与 MSP 配置
- `system/`：启动、系统文件与运行时支撑

实现顺序建议：

1. 先建立最小启动与 `UART` 输出
2. 再补 `PWM` 输出
3. 最后再做参数调整和展示优化

当前结构已把 `display.c` 作为统一门面，`display_uart.c` 保持稳定串口输出，后续真实 `RGBLCD/LTDC` 驱动继续放在 `display/` 下补齐即可。
