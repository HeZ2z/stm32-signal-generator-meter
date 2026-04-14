# Pin Plan

## 板卡确认信息

| 项目 | 当前状态 | 备注 |
| --- | --- | --- |
| 开发板型号 | Apollo STM32 F4/F7 | 已知 |
| 准确 MCU 型号 | TBD | 必须优先确认 |
| 调试器类型 | TBD | 例如 ST-LINK |
| 串口转接方式 | TBD | 板载 / 外接 |

## 关键功能引脚规划

| 功能 | 外设角色 | 候选引脚 | 外设实例 | 状态 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `PWM_OUT` | 波形输出 | TBD | `TIMx_CHy` | Pending | 需要和输入捕获形成回环 |
| `CAP_IN` | 输入捕获 | TBD | `TIMx_CHy` | Pending | 推荐用杜邦线与 `PWM_OUT` 相连 |
| `UART_TX` | 串口发送 | TBD | `USARTx_TX` | Pending | 优先保证日志输出 |
| `UART_RX` | 串口接收 | TBD | `USARTx_RX` | Pending | 串口命令使用 |
| `LED` | 状态指示 | TBD | `GPIO` | Optional | 用于上电或错误提示 |
| `KEY` | 本地输入 | TBD | `GPIO/EXTI` | Optional | 非首版必需 |

## 确认顺序建议

1. 先确认准确 `MCU`
2. 再确认可用 `UART`
3. 再确认适合做 `PWM` 输出的定时器通道
4. 再确认适合做输入捕获的定时器通道
5. 最后确认是否需要按键或 `LED`

## 回环连接说明

- 使用杜邦线将 `PWM_OUT` 连接到 `CAP_IN`
- 地线默认共地，无需额外处理
- 若板载电平或复用冲突，需要在此文档补充说明
