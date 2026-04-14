# stm32-signal-generator-meter

单板 `STM32F4/F7` 实践项目，当前主线目标是在一周周期内完成一个可演示、可答辩、可稳定运行的 `PWM 输出 + 输入测量 + UART 输出` 闭环固件。

## 项目状态

- 阶段：`M5 finish and polish`
- 开发板：`Apollo STM32 F4/F7`
- 当前硬件条件：`仅开发板本体，无外接传感器`
- 当前策略：按 `STM32F429` 路线收敛主固件，优先完成板内回环测量闭环
- 当前未决项：板载下载调试链路、实板最终 `TIM/UART` 引脚可用性

当前仓库已提供一个基于 `STM32F429 + HAL + CMake` 的主固件，当前主路径包含 `UART + PWM + TIM PWM Input + 串口命令`，时钟默认走 `HSI 16 MHz`，并把板级引脚集中在 `include/board_config.h` 中，便于后续按实物统一修正。
当前仓库还补充了误差样例和演示脚本文档，便于直接用于收尾、答辩和演示。

## 项目目标

在单块开发板上完成如下链路：

`参数设置 -> 信号输出 -> 信号输入 -> 参数测量 -> 结果显示`

当前主固件优先交付：

1. 一个可配置的 `PWM` 方波输出
2. 测量频率、周期和占空比
3. 至少一种参数调整方式：串口命令
4. 通过 `UART` 输出设定值和实测值

## 为什么选这个题目

- 不依赖外接传感器或外部信号源
- 只用单板和一根杜邦线即可完成板内回环验证
- 能覆盖 `TIM`、`PWM`、`Input Capture`、`UART` 等核心 STM32 能力
- 最小可行版本收敛快，适合一周周期
- 既能演示完整链路，也便于后续扩展和答辩解释

## 当前最小可行范围

当前主线只做最小可演示固件：

1. 输出可配置频率和占空比的 `PWM`
2. 通过 `TIM PWM Input` 测量回环信号
3. 通过串口命令修改参数
4. 串口同时打印当前设定值和实测值
5. 用 `LED` 心跳证明主循环稳定运行
6. 用宿主机侧小测试覆盖命令解析、`PWM` 参数换算和测量结果换算逻辑

推荐验证方式：

- 使用杜邦线连接 `PB6(TIM4_CH1)` 到 `PB5(TIM3_CH2)`
- 用串口工具观察 `SET` 与 `MEAS` 输出
- 用串口命令修改频率和占空比

## 技术路线

- 输出路径：`TIM + PWM`
- 输入路径：`TIM PWM Input`
- 控制路径：`UART 命令` 优先，按键作为补充
- 显示路径：`UART` 优先，复杂显示放到后续
- 软件结构：
  - `signal_gen`：信号产生
  - `signal_measure`：输入测量
  - `ui_ctrl`：参数设置
  - `display`：串口输出
  - `app`：调度与状态管理

## 开发里程碑

- `M0` 仓库骨架与文档初始化
- `M1` 确认准确 `MCU` 型号、调试器、串口和定时器资源
- `M2` 跑通最小工程与 `UART` 日志输出
- `M3` 跑通 `PWM` 输出
- `M4` 跑通输入测量闭环
- `M5` 做误差分析、边界测试和演示材料整理

详细计划见 [docs/roadmap.md](docs/roadmap.md)，误差分析见 [docs/error-analysis.md](docs/error-analysis.md)，演示流程见 [docs/demo-script.md](docs/demo-script.md)。

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

6. 先验证 `UART`，再验证 `PWM` 和 `PB6 -> PB5` 回环测量
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

8. 串口观察 `SET freq=... duty=... | MEAS ...` 输出是否跟随设定变化
9. 每次阶段性完成后更新 [docs/verification-log.md](docs/verification-log.md)

## CI

仓库已提供 GitHub Actions 工作流 [ci.yml](.github/workflows/ci.yml)，当前会在 `main` 的 `push` 和 `pull_request` 上执行：

- 宿主机侧 `CTest`
- `arm-none-eabi` 交叉编译固件
- 上传 `firmware.elf/.hex/.bin/.map` 构建产物

这条 CI 主线只验证“主固件可交叉编译 + 宿主机逻辑测试”，不包含板级烧录和硬件联调。

## 当前非目标

- 引入外接传感器
- 在最小闭环跑通前扩展到复杂显示模块
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
