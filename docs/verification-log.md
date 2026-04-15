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
| 2026-04-14 | `M4` | 板级回环默认测量 | `sudo python3 tools/serial_monitor.py --port /dev/ttyUSB0 --baud 115200` 持续输出 `SET freq=1000Hz duty=50% | MEAS freq=1001Hz period=999us duty=50%` | Pass | 早期验证使用 `PB6(TIM4_CH1) -> PB5(TIM3_CH2)`，后续已迁移到 `PA7` |
| 2026-04-14 | `M4` | 板级断线退化行为 | 拔掉回环线后持续输出 `SET freq=1000Hz duty=50% | MEAS no-signal` | Pass | 无信号超时逻辑工作正常；串口打开瞬间的 `ERR unknown command` 视为残留字符，不影响测量结论 |
| 2026-04-14 | `M4` | `2000/30` 参数联调 | 串口命令返回 `OK freq=2000`、`OK duty=30`，随后稳定输出 `SET freq=2000Hz duty=30% | MEAS freq=2004Hz period=499us duty=30%` | Pass | `2000/30` 场景实测闭环正常，频率与周期误差符合 `1 MHz` 计数分辨率预期 |
| 2026-04-14 | `M5` | 误差摘要串口输出 | 默认回环持续输出 `SET freq=1000Hz duty=50% | MEAS freq=1001Hz period=999us duty=50% | ERR df=1Hz dt=-1us dd=0%` | Pass | `M5` 状态行已能直接展示频率、周期、占空比误差 |
| 2026-04-14 | `M5` | `2000/30` 误差样例 | 持续输出 `SET freq=2000Hz duty=30% | MEAS freq=2004Hz period=499us duty=30% | ERR df=4Hz dt=-1us dd=0%` | Pass | 误差摘要与原始 `MEAS` 一致，可直接用于答辩说明 |
| 2026-04-14 | `M5` | `5000/70` 误差样例 | 持续输出 `SET freq=5000Hz duty=70% | MEAS freq=5025Hz period=199us duty=70% | ERR df=25Hz dt=-1us dd=0%` | Pass | 高频场景下仍保持占空比一致，频率误差符合 `1 MHz` 计数分辨率预期 |
| 2026-04-15 | `M6` | LCD 集成代码构建 | `cmake --build build` | Pass | 已接入 `LTDC + SDRAM + RGB565` 最小 LCD 状态页实现，待实板点屏验证 |
| 2026-04-15 | `M6` | 宿主机逻辑回归 | `ctest --test-dir build-host-tests --output-on-failure` | Pass | LCD 集成未改变命令解析、PWM 换算与测量逻辑测试结果 |
| 2026-04-15 | `M6` | 回环迁脚方案 | 文档与固件统一改为 `PB6(TIM4_CH1) -> PA7(TIM3_CH2)` | Pass | `PB5` 已释放给 `LCD_BL`，后续实板验证必须按新回环线连接 |
| 2026-04-15 | `M6` | Apollo LCD/SDRAM 真值收敛 | 代码与文档统一改为 `SDRAM Bank1 + 0xC0000000 + 4342 1/1/40/8/5/8` | Pass | 已移除错误的 `Bank2 + 0xD0000000` 假设，等待实板继续验证 |
| 2026-04-15 | `M7` | 触摸调参集成代码构建 | `cmake --build build` | Pass | 已切换到 `GT9xxx` 电容触摸、LCD 按键页和 UART 可写兜底路径，待实板验证触摸坐标方向与按键命中 |
| 2026-04-15 | `M7` | GT9xxx 触摸控制器识别 | 串口输出 `TOUCH GT9XXX READY ID=9147` | Pass | 说明 `GT9147` 控制器已成功初始化并可读 |
| 2026-04-15 | `M7` | 触摸状态位修复后恢复报点 | 修正 `GT9xxx` 状态清除逻辑后，串口临时调试输出出现 `touch raw ...` 与 `touch event=...` | Pass | 根因确认为空闲 `0x80` 状态未及时清除，修复后已恢复触摸事件链路 |
| 2026-04-15 | `M7` | LCD 触摸调参联调 | 触摸后串口输出从 `SET freq=1000Hz duty=50%` 变为 `SET freq=20Hz duty=50%`、`SET freq=100000Hz duty=50%`、`SET freq=100000Hz duty=55%` | Pass | 说明 LCD 触摸按键已能实际触发参数修改 |
| 2026-04-15 | `M7` | 清理调试日志后固件重编译 | `cmake --build build` | Pass | 已移除 `touch raw`、`touch event` 和原始寄存器调试刷屏，只保留摘要状态输出 |
| 2026-04-15 | `M7` | 推荐演示频段确认 | `1000/50`、`2000/30`、`5000/70` 已有稳定样例；`20Hz` 与 `100000Hz` 仅作为边界现象观察 | Pass | 中频段适合主演示，极端频率用于说明当前 `1 MHz` 计时基准下的量化边界 |
| 2026-04-15 | `M7` | 触摸连点抑制 | 代码新增 `30ms` 按下去抖、`45ms` 释放去抖、`12px` 候选漂移阈值 | Pass | 目标是抑制 GT9xxx 边缘抖动造成的重复点击，待实板继续确认手感 |
| 2026-04-15 | `M7` | `MORE` 项目信息弹层接入 | 代码新增 `more_open` 状态、LCD overlay 和触摸屏蔽逻辑 | Pass | `MORE` 与 UART `help` 已解耦；待实板确认开关手感、遮罩效果和无频闪表现 |

## 后续记录建议

- 编译成功时记录工具链、命令和产物
- 烧录成功时记录烧录工具和连接方式
- 串口验证时记录串口参数、输出样例和观察结果
- 测量验证时记录设定值、实测值和误差说明
