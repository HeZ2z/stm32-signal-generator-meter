# Include Layout

本目录预留给公共头文件和模块接口定义。

当前公共接口已按模块分目录：

- `app/`：应用调度和状态机接口
- `board/`：板级配置接口
- `display/`：显示接口
- `signal_gen/`：`PWM` 输出与参数配置接口
- `signal_measure/`：输入测量接口
- `ui/`：串口命令与控制接口

当前主固件板级强绑定配置集中在 `board/board_config.h`，后续修板时优先修改该文件而不是分散改动各模块。
