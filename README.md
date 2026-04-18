# stm32-signal-generator-meter

单板 `STM32F4/F7` 实践项目。当前仓库已经完成 `PWM 输出 + 输入测量 + UART/LCD/触摸 + 单通道真实波形显示` 闭环固件，并进入下一阶段规划：在保留旧主线可编译、可回归的前提下，继续扩展 `双 DAC 输出 + 双通道采样 + XY/YT 显示` 演示链路。

## 项目状态

- 阶段：`M8 已完成，M9-M11 已规划`
- 开发板：`Apollo STM32 F4/F7`
- 当前硬件条件：`仅开发板本体，无外接传感器`
- 当前策略：按 `STM32F429` 路线收敛主固件，并在现有 PWM/测量主线上并行扩展 DAC/ADC 波形显示主线
- 当前未决项：按 `M9-M11` 推进双通道输出与 `XY/YT` 演示
  - 当前 LCD 真值基线：`Apollo STM32F429IGT6 + SDRAM Bank1 + framebuffer 0xC0000000`
  - 已落地屏参：`480x272 RGB565`, `9MHz`, `HSYNC 1 / HBP 40 / HFP 5 / VSYNC 1 / VBP 8 / VFP 8`
  - 已完成冲突处理：`PB5` 固定改作 `LCD_BL`，测量输入迁到 `PA7(TIM3_CH2)`
  - 已落地触摸基线：`GT9147/GT9xxx`, `PH6=SCL`, `PI3=SDA`, `PI8=RST`, `PH7=INT`

当前仓库已提供一个基于 `STM32F429 + HAL + CMake` 的主固件，当前主路径包含 `UART + PWM + TIM PWM Input + 串口命令`，时钟默认走 `HSI 16 MHz`，并把板级引脚集中在 `include/board_config.h` 中，便于后续按实物统一修正。
当前仓库还补充了误差样例和演示脚本文档，并把显示层升级为 `UART` 主通道 + `RGBLCD` 波形页后端 + `GT9xxx` 触摸调参入口。下一阶段会在现有单通道真实波形页基础上，逐步加入双路 DAC、双通道采样和 `XY/YT` 图形显示。

## 项目目标

在单块开发板上完成如下链路：

`参数设置 -> 信号输出 -> 信号输入 -> 参数测量 -> 结果显示`

当前已交付主固件：

1. 一个可配置的 `PWM` 方波输出
2. 测量频率、周期和占空比
3. 至少一种参数调整方式：串口命令
4. 通过 `UART` 输出设定值和实测值
5. 为 `ALIENTEK 4342 RGBLCD` 提供并行显示后端，并保留 `UART` 调试兜底
6. 通过 `GT9xxx` 电容触摸在 LCD 上直接调整频率和占空比
7. 通过 `ADC1 + DMA` 在 LCD 上显示单通道真实采样 `YT` 方波

当前规划继续交付：

8. 双路 `DAC` 波形输出
9. 双通道 `ADC + DMA` 回采
10. `XY` 模式与李萨如图形演示

## 为什么选这个题目

- 不依赖外接传感器或外部信号源
- 只用单板和一根杜邦线即可完成板内回环验证
- 能覆盖 `TIM`、`PWM`、`Input Capture`、`UART` 等核心 STM32 能力
- 最小可行版本收敛快，适合一周周期
- 既能演示完整链路，也便于后续扩展和答辩解释

## 当前最小可行范围

当前已完成的最小可演示固件：

1. 输出可配置频率和占空比的 `PWM`
2. 通过 `TIM PWM Input` 测量回环信号
3. 通过串口命令或 LCD 触摸修改参数
4. 串口同时打印当前设定值和实测值
5. 用 `LED` 心跳证明主循环稳定运行
6. 用宿主机侧小测试覆盖命令解析、`PWM` 参数换算和测量结果换算逻辑

当前主线推荐验证方式：

- 使用杜邦线连接 `PB6(TIM4_CH1)` 到 `PA7(TIM3_CH2)`
- 用串口工具观察 `SET` 与 `MEAS` 输出
- 用串口命令修改频率和占空比

## 技术路线

- 输出路径：当前为 `TIM + PWM`，后续扩展 `DAC`
- 输入路径：当前并行包含 `TIM PWM Input` 和 `ADC + DMA` 单通道真实采样，后续扩展双通道 `ADC + DMA`
- 控制路径：`UART 命令 + GT9xxx 触摸按键`
- 显示路径：`UART` 主通道，`ALIENTEK 4342 RGBLCD` 已接入真实采样 `YT` 波形页与触摸界面，后续扩展双通道 `YT / XY`
- 软件结构：
  - `signal_gen`：当前 PWM 信号产生
  - `signal_measure`：当前输入测量
  - `signal_gen_dac`：后续双 DAC 波形产生
  - `signal_capture_adc`：当前单通道 ADC 采样，后续扩展双通道
  - `scope_render`：当前单通道 `YT` 绘图，后续扩展 `XY`
  - `ui_ctrl`：参数设置
- `display`：统一显示接口，当前已接入 `UART` 与 `RGBLCD` 波形页后端
  - `app`：调度与状态管理

## 开发里程碑

- `M0` 仓库骨架与文档初始化
- `M1` 确认准确 `MCU` 型号、调试器、串口和定时器资源
- `M2` 跑通最小工程与 `UART` 日志输出
- `M3` 跑通 `PWM` 输出
- `M4` 跑通输入测量闭环
- `M5` 参数调整、输出格式和误差展示收尾
- `M6` 集成 `ALIENTEK 4342 RGBLCD`
- `M7` 集成 `GT9xxx` 触摸调参
- `M8` 单通道真实采样 `YT` 波形显示
- `M9` 双 DAC 双方波 + 双通道 `YT`
- `M10` 三角波 + 最小 `XY`
- `M11` 正弦波 + 李萨如演示收口

## 推荐演示范围

- 当前已完成主演示参数：`1000/50`、`2000/30`、`5000/70`
- 当前已完成主演示链路：默认输出 -> 触摸改参数 -> 串口/屏幕同步显示 -> 拔线展示 `no-signal`
- 后续主演示链路：`YT` 波形显示 -> 双通道叠加 -> `XY` 模式 -> 李萨如图形
- `20Hz` 和 `100000Hz` 仅作为边界现象观察，不建议作为主演示参数

## 当前实现边界

- 当前 `PWM` 发生与输入测量统一使用 `1 MHz` 计时基准，最小时间分辨率为 `1 us`
- 中频段演示表现稳定，极端高频下每周期 tick 数过少，频率和占空比量化误差会明显放大
- 极端低频切换时，`SET` 已更新而 `MEAS` 可能短暂保留上一帧或退化为 `no-signal`，这是当前状态刷新节奏与超时策略决定的
- `M8` 的真实采样显示固定先走 `PB6(TIM4_CH1) -> PA0(ADC1_IN0)` 单通道回采
- `M8` 已完成 `display_lcd.c` 按技术层拆分（`lcd_prim.c` 绘图原语 + `lcd_font.c` 字模），`display_lcd.c` 保留场景编排与 LTDC/SDRAM 初始化
- `M8` 左侧 `INPUT` 卡固定表示 `PA0 ADC1` 真实采样输入，不再混用 `PA7 TIM3` 测量结果
- 当前波形页优先针对方波输入优化：频率窗内显示真实输入 `F/D`，超出可稳定估算窗口时显示 `F=<当前输出频率> D=--`
- 当前断线/悬空输入依赖 `PA0` 下拉和更严格的方波判定退化为无效输入，避免随机 `F/D`
- 当前波形区仍采用 `CPU + RGB565` 单层绘制，通过局部刷新、短保持和失败滞回抑制频闪与偶发 `ADC LIVE`
- `M9` 起正式切到 `PA4(DAC1) -> PA0`、`PA5(DAC2) -> PA1` 双通道模拟链路
- `XY` 最小版计划在 `M10` 接入，`M11` 收口为正式答辩演示
- 双通道采样首版接受顺序扫描的非严格同时采样限制，主演示频段会优先控制在中低频范围

详细计划见 [docs/roadmap.md](docs/roadmap.md)，误差分析见 [docs/error-analysis.md](docs/error-analysis.md)，演示流程见 [docs/demo-script.md](docs/demo-script.md)。
LCD 集成分析见 [docs/lcd-integration-notes.md](docs/lcd-integration-notes.md)。

## 仓库结构

```text
.
├── AGENTS.md
├── CHANGELOG.md
├── CONTRIBUTING.md
├── LICENSE
├── README.md
├── cmake/
├── docs/
├── include/
├── src/
├── tests/
└── tools/
```

- `docs/`：设计说明、引脚规划、路线图、验证记录、决策记录
- `include/`：公共头文件与模块接口的规划位置
- `src/`：核心源码实现位置
- `tests/`：宿主机侧最小回归测试
- `cmake/`：后续工具链与构建辅助文件位置
- `tools/`：烧录、串口、环境检查等辅助脚本位置

`build/` 作为本地构建输出目录使用，不纳入版本控制。

## 快速开始

1. 确认开发板上的准确 `MCU` 型号和调试接口
2. 在 [docs/pin-plan.md](docs/pin-plan.md) 中记录 `UART`、`PWM`、输入捕获候选引脚
3. 在 [docs/decision-log.md](docs/decision-log.md) 中记录最终采用 `F4` 还是 `F7`
4. 本机构建最小工程：

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake -G Ninja
cmake --build build
```

5. 串口监听：

```bash
python3 tools/serial_monitor.py --port /dev/ttyUSB0 --baud 115200
```

6. 先验证 `UART`，再验证 `PWM`、`PB6 -> PA7` 回环测量和 LCD 触摸按键
7. 先跑宿主机逻辑测试：

```bash
./tests/run_host_tests.sh
```

或使用 `ctest`：

```bash
cmake -S tests -B build-host-tests
cmake --build build-host-tests
ctest --test-dir build-host-tests --output-on-failure
```

8. 串口观察 `SET freq=... duty=... | MEAS ...` 输出是否跟随设定变化，LCD 触摸按键是否能触发参数调整
9. 每次阶段性完成后更新 [docs/verification-log.md](docs/verification-log.md)

## CI

仓库已提供 GitHub Actions 工作流 [ci.yml](.github/workflows/ci.yml)，当前会在 `main` 的 `push` 和 `pull_request` 上执行：

- 宿主机侧 `CTest`
- `arm-none-eabi` 交叉编译固件
- 上传 `firmware.elf/.hex/.bin/.map` 构建产物

这条 CI 主线只验证“主固件可交叉编译 + 宿主机逻辑测试”，不包含板级烧录和硬件联调。

## 当前非目标

- 引入外接传感器
- 在缺少真实板级资料时猜测 `RGBLCD` 时序和引脚参数
- 为了功能“看起来多”而牺牲一周周期内的可交付性

## 文档索引

- [docs/README.md](docs/README.md)：文档导航
- [docs/architecture.md](docs/architecture.md)：系统结构与模块边界
- [docs/pin-plan.md](docs/pin-plan.md)：引脚与外设规划
- [docs/roadmap.md](docs/roadmap.md)：阶段计划
- [docs/verification-log.md](docs/verification-log.md)：验证记录
- [docs/decision-log.md](docs/decision-log.md)：关键决策记录
- [docs/error-analysis.md](docs/error-analysis.md)：误差样例与分析
- [docs/demo-script.md](docs/demo-script.md)：演示脚本

## 贡献说明

欢迎在不破坏当前约束的前提下改进本项目。提交前请至少确保：

- 设计假设写入文档
- 重要改动附带验证方法或结果
- 如果题目、硬件条件、周期发生变化，同步更新 `README.md` 与 `AGENTS.md`

更多约定见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

本仓库当前使用 [MIT License](LICENSE)。
