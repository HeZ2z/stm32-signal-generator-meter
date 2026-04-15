# Pin Plan

## 板卡确认信息

| 项目 | 当前状态 | 备注 |
| --- | --- | --- |
| 开发板型号 | Apollo STM32 F4/F7 | 已知 |
| 准确 MCU 型号 | `STM32F429IGT6` | 当前主固件按 Apollo `V15` 路线实现 |
| 调试器类型 | TBD | 当前只确认了串口链路 |
| 串口转接方式 | 板载 `CH340` | `USB 232 -> /dev/ttyUSB0` 已验证 |

## 关键功能引脚规划

| 功能 | 外设角色 | 候选引脚 | 外设实例 | 状态 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `PWM_OUT` | 波形输出 | `PB6` | `TIM4_CH1` | Active | 当前稳定主固件默认输出脚 |
| `MEAS_IN` | 输入测量 | `PA7` | `TIM3_CH2` | Active | 为释放 `PB5` 给 `LCD_BL`，回环改为 `PB6 -> PA7` |
| `UART_TX` | 串口发送 | `PA9` | `USART1_TX` | Provisional | 当前工程默认日志口 |
| `UART_RX` | 串口接收 | `PA10` | `USART1_RX` | Provisional | 当前工程默认命令口 |
| `LCD_BL` | 背光控制 | `PB5` | `GPIO` | Active | 与旧 `MEAS_IN` 冲突，已由 M6 迁脚消解 |
| `LCD_RGB` | 屏幕数据/同步 | `PI12-15, PJ0-15, PK0-7` | `LTDC` | Active | 当前按 Apollo F429 RGBLCD 接口接入 |
| `SDRAM` | 帧缓冲存储 | `PC0/2/3, PD, PE, PF, PG` | `FMC SDRAM Bank1` | Active | `PC0=SDNWE`, `PC2=SDNE0`, `PC3=SDCKE0`，帧缓冲基地址固定为 `0xC0000000` |
| `TOUCH_GT9XXX` | 电容触摸控制 | `PH6/PI3/PI8/PH7` | `GPIO bit-bang I2C` | Active | `PH6=SCL`, `PI3=SDA`, `PI8=RST`, `PH7=INT` |
| `LED` | 状态指示 | `PB0/PB1` | `GPIO` | Active | 保留心跳与错误闪烁用途 |
| `KEY` | 本地输入 | TBD | `GPIO/EXTI` | Optional | 非首版必需 |

## 确认顺序建议

1. 先确认准确 `MCU`
2. 再确认可用 `UART`
3. 确认 `PB6 -> PA7` 回环布线可用
4. 最后确认是否需要按键或 `LED`

当前仓库中的主固件映射见 `include/board_config.h`。
