# Pin Plan

## 板卡确认信息

| 项目 | 当前状态 | 备注 |
| --- | --- | --- |
| 开发板型号 | Apollo STM32 F4/F7 | 已知 |
| 准确 MCU 型号 | `STM32F429` | 当前主固件按此路线实现 |
| 调试器类型 | TBD | 当前只确认了串口链路 |
| 串口转接方式 | 板载 `CH340` | `USB 232 -> /dev/ttyUSB0` 已验证 |

## 关键功能引脚规划

| 功能 | 外设角色 | 候选引脚 | 外设实例 | 状态 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `PWM_OUT` | 波形输出 | `PB6` | `TIM4_CH1` | Active | 当前稳定主固件默认输出脚 |
| `MEAS_IN` | 输入测量 | `PB5` | `TIM3_CH2` | Active | 当前 `TIM PWM Input` 默认输入脚，回环接 `PB6` |
| `UART_TX` | 串口发送 | `PA9` | `USART1_TX` | Provisional | 当前工程默认日志口 |
| `UART_RX` | 串口接收 | `PA10` | `USART1_RX` | Provisional | 当前工程默认命令口 |
| `LED` | 状态指示 | TBD | `GPIO` | Optional | 用于上电或错误提示 |
| `KEY` | 本地输入 | TBD | `GPIO/EXTI` | Optional | 非首版必需 |

## 确认顺序建议

1. 先确认准确 `MCU`
2. 再确认可用 `UART`
3. 确认 `PB6 -> PB5` 回环布线可用
4. 最后确认是否需要按键或 `LED`

当前仓库中的主固件映射见 `include/board_config.h`。
