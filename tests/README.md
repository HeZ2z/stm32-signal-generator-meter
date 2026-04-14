# Tests

本目录保存不依赖开发板即可运行的最小回归测试。

当前测试范围：

- `test_ui_cmd.c`：串口命令解析与命令缓冲行为
- `test_signal_gen_logic.c`：`PWM` 参数边界与定时器换算逻辑
- `run_host_tests.sh`：在宿主机上编译并运行上述测试

运行方式：

```bash
./tests/run_host_tests.sh
```

当前策略：

- 只测试纯逻辑，不做板级自动化
- 优先覆盖容易回归的命令解析和参数换算
- 硬件链路仍以构建、烧录和串口联调记录为主
