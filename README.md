# STM32 Signal Generator & Frequency Meter

单板 `STM32F4/F7` 实践项目，目标是在一周周期内完成一个可演示、可答辩、可扩展的“信号发生与频率测量一体机”。

## 项目状态

- 阶段：`bootstrap / architecture`
- 开发板：`Apollo STM32 F4/F7`
- 当前硬件条件：`仅开发板本体，无外接传感器`
- 当前策略：优先搭好仓库骨架、设计文档和验证路径，再确认具体 `MCU` 型号并落地外设代码
- 当前未决项：准确芯片型号、可用 `TIM/UART` 引脚、调试器链路、首选工具链

当前不会提前写死 `F4` 或 `F7` 的初始化细节，也不会在未确认引脚映射前硬编码时钟和外设配置。

## 项目目标

在单块开发板上完成如下链路：

`参数设置 -> 信号输出 -> 信号输入 -> 参数测量 -> 结果显示`

首版优先交付：

1. 一个可配置的 `PWM` 方波输出
2. 一个输入通道对该信号进行测量
3. 频率、周期、占空比计算
4. 通过 `UART` 输出设定值与实测值
5. 至少一种参数调整方式：按键或串口命令

## 为什么选这个题目

- 不依赖外接传感器或外部信号源
- 只用单板和一根杜邦线即可完成板内回环验证
- 能覆盖 `TIM`、`PWM`、`Input Capture`、`UART` 等核心 STM32 能力
- 最小可行版本收敛快，适合一周周期
- 既能演示完整链路，也便于后续扩展和答辩解释

## 当前最小可行范围

第一阶段只做最小闭环：

1. 输出固定频率和占空比的 `PWM`
2. 用另一输入脚做输入捕获测量
3. 串口打印频率、周期、占空比
4. 通过串口命令或按键修改参数

推荐验证方式：

- 用杜邦线将 `PWM 输出引脚` 接到 `输入捕获引脚`
- 用串口助手观察设定值与实测值

## 技术路线

- 输出路径：`TIM + PWM`
- 测量路径：`TIM Input Capture` 或 `PWM Input`
- 控制路径：`UART 命令` 优先，按键作为补充
- 显示路径：`UART` 优先，复杂显示放到后续
- 软件结构：
  - `signal_gen`：信号产生
  - `signal_measure`：参数测量
  - `ui_ctrl`：参数设置
  - `display`：串口输出
  - `app`：调度与状态管理

## 开发里程碑

- `M0` 仓库骨架与文档初始化
- `M1` 确认准确 `MCU` 型号、调试器、串口和定时器资源
- `M2` 跑通最小工程与 `UART` 日志输出
- `M3` 跑通 `PWM` 输出
- `M4` 跑通输入捕获并完成频率/周期/占空比计算
- `M5` 实现参数调整与设定值/实测值联调
- `M6` 做误差分析、边界测试和演示材料整理

详细计划见 [docs/roadmap.md](docs/roadmap.md)。

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
└── tools/
```

- `docs/`：设计说明、引脚规划、路线图、验证记录、决策记录
- `include/`：公共头文件与模块接口的规划位置
- `src/`：核心源码实现位置
- `cmake/`：后续工具链与构建辅助文件位置
- `tools/`：烧录、串口、环境检查等辅助脚本位置

`build/` 作为本地构建输出目录使用，不纳入版本控制。

## 快速开始

1. 确认开发板上的准确 `MCU` 型号和调试接口
2. 在 [docs/pin-plan.md](docs/pin-plan.md) 中记录 `UART`、`PWM`、输入捕获候选引脚
3. 在 [docs/decision-log.md](docs/decision-log.md) 中记录最终采用 `F4` 还是 `F7`
4. 生成最小工程，优先只打开 `GPIO + UART + TIM`
5. 先验证 `UART`，再验证 `PWM`，最后完成板内回环测量
6. 每次阶段性完成后更新 [docs/verification-log.md](docs/verification-log.md)

## 当前非目标

- 引入外接传感器
- 在最小闭环跑通前扩展到复杂显示模块
- 在未确认具体芯片前提交大量与型号强耦合的初始化代码
- 为了功能“看起来多”而牺牲一周周期内的可交付性

## 文档索引

- [docs/README.md](docs/README.md)：文档导航
- [docs/architecture.md](docs/architecture.md)：系统结构与模块边界
- [docs/pin-plan.md](docs/pin-plan.md)：引脚与外设规划
- [docs/roadmap.md](docs/roadmap.md)：阶段计划
- [docs/verification-log.md](docs/verification-log.md)：验证记录
- [docs/decision-log.md](docs/decision-log.md)：关键决策记录

## 贡献说明

欢迎在不破坏当前约束的前提下改进本项目。提交前请至少确保：

- 设计假设写入文档
- 重要改动附带验证方法或结果
- 如果题目、硬件条件、周期发生变化，同步更新 `README.md` 与 `AGENTS.md`

更多约定见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

本仓库当前使用 [MIT License](LICENSE)。
