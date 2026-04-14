# Verification Log

用于记录阶段性验证证据，避免“感觉已经完成”但没有可追溯结果。

| 日期 | 阶段 | 验证项 | 证据 | 结果 | 备注 |
| --- | --- | --- | --- | --- | --- |
| 2026-04-14 | `M0` | 仓库骨架整理 | README、docs、协作文件已建立 | Pass | 尚未进入硬件编译与烧录阶段 |
| 2026-04-14 | `M1` | Ubuntu 串口识别 | `USB 232` 识别为 `CH340`，节点 `/dev/ttyUSB0` | Pass | 当前用户已加入 `dialout` |
| 2026-04-14 | `M1` | 串口可打开 | `python3` 成功打开 `/dev/ttyUSB0`，`opened: True` | Pass | 当前无板载用户程序输出 |
| 2026-04-14 | `M1` | 工具链安装 | 已安装 `arm-none-eabi-gcc`、`cmake`、`ninja`、`openocd`、`pyserial` | Pass | `CubeMX` 仍待手动安装 |
| 2026-04-14 | `M1` | 最小工程构建 | `cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake -G Ninja && cmake --build build` | Pass | 生成 `build/firmware.elf/.hex/.bin` |
| 2026-04-14 | `M2` | 主固件稳定收口 | 仅保留 `UART + PWM + 串口命令 + LED` | Pass | 测量链路已从主固件移除，后续单独实验 |
| 2026-04-14 | `M2` | 串口命令联调 | `picocom` 下 `freq 2000`、`duty 30` 返回 `OK ...` | Pass | 主固件交互链路已可演示 |
| 2026-04-14 | `M2` | 宿主机逻辑回归测试 | `./tests/run_host_tests.sh` 输出 `PASS: test_ui_cmd`、`PASS: test_signal_gen_logic` | Pass | 覆盖命令解析、缓冲溢出、PWM 参数边界和换算 |
| 2026-04-14 | `M2` | `CTest` 测试入口 | `cmake -S tests -B build-host-tests && ctest --test-dir build-host-tests --output-on-failure` | Pass | 宿主机测试已具备标准化入口 |
| 2026-04-14 | `M2` | 测试改动后固件重编译 | `cmake --build build` | Pass | 测试重构后主固件仍可生成 `firmware.elf` |

## 后续记录建议

- 编译成功时记录工具链、命令和产物
- 烧录成功时记录烧录工具和连接方式
- 串口验证时记录串口参数、输出样例和观察结果
- 测量验证时记录设定值、实测值和误差说明
