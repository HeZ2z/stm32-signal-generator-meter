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
| 2026-04-14 | `M4` | 测量逻辑宿主机回归测试 | `cmake -S tests -B build-host-tests -G Ninja && ctest --test-dir build-host-tests --output-on-failure` | Pass | 新增 `test_signal_measure_logic`，覆盖 `2000/30`、`1000/50`、`5000/70` 与异常输入 |
| 2026-04-14 | `M4` | 主固件集成编译 | `cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake -G Ninja && cmake --build build` | Pass | `signal_measure`、`TIM3 IRQ`、`SET/MEAS` 串口输出已编译通过 |
| 2026-04-14 | `M4` | 板级回环默认测量 | `sudo python3 tools/serial_monitor.py --port /dev/ttyUSB0 --baud 115200` 持续输出 `SET freq=1000Hz duty=50% | MEAS freq=1001Hz period=999us duty=50%` | Pass | `PB6(TIM4_CH1) -> PB5(TIM3_CH2)` 回环工作正常，误差符合 `1 MHz` 计数分辨率预期 |
| 2026-04-14 | `M4` | 板级断线退化行为 | 拔掉回环线后持续输出 `SET freq=1000Hz duty=50% | MEAS no-signal` | Pass | 无信号超时逻辑工作正常；串口打开瞬间的 `ERR unknown command` 视为残留字符，不影响测量结论 |
| 2026-04-14 | `M4` | `2000/30` 参数联调 | 串口命令返回 `OK freq=2000`、`OK duty=30`，随后稳定输出 `SET freq=2000Hz duty=30% | MEAS freq=2004Hz period=499us duty=30%` | Pass | `2000/30` 场景实测闭环正常，频率与周期误差符合 `1 MHz` 计数分辨率预期 |

## 后续记录建议

- 编译成功时记录工具链、命令和产物
- 烧录成功时记录烧录工具和连接方式
- 串口验证时记录串口参数、输出样例和观察结果
- 测量验证时记录设定值、实测值和误差说明
