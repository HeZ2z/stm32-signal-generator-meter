# LCD Integration Notes

## 当前基线

当前 `ALIENTEK 4342 RGBLCD` 集成以 Apollo `STM32F429IGT6` 板级资料为准，不再沿用其他开发板或同系列面板的经验参数。

当前代码基线：

- `MCU`: `STM32F429IGT6`
- `LCD`: `ALIENTEK 4342 RGBLCD`
- `Framebuffer`: `0xC0000000`
- `SDRAM`: `FMC Bank1`
- `Pixel format`: `RGB565`
- `UART`: 保留为主调试通道

## Apollo 硬件真值

来自 `阿波罗V2 硬件参考手册_V1.0（F429版本）.pdf`：

- `PB5 = LCD_BL`
- `PB6` 未连接板载设备，可继续作为 `PWM_OUT`
- `PC0 = FMC_SDNWE`
- `PC2 = FMC_SDNE0`
- `PC3 = FMC_SDCKE0`
- `PF11 = FMC_SDNRAS`
- `PG15 = FMC_SDNCAS`
- `PG8 = FMC_SDCLK`
- `PG4 = FMC_BA0`
- `PG5 = FMC_BA1`

直接结论：

- `SDRAM` 必须走 `Bank1`
- 帧缓冲基地址必须使用 `0xC0000000`
- `PB5` 不能再作为测量输入，回环固定改为 `PB6 -> PA7`

## Apollo LTDC 参数

来自 `STM32F429 阿波罗开发指南V1.1.pdf` 第 444 到 475 页，对 `0x4342` 面板的参数如下：

- 分辨率：`480x272`
- 像素时钟：`9MHz`
- `HSW = 1`
- `VSW = 1`
- `HBP = 40`
- `VBP = 8`
- `HFP = 5`
- `VFP = 8`
- `HSPolarity = AL`
- `VSPolarity = AL`
- `DEPolarity = AL`
- `PCPolarity = IPC`
- 显存地址：`0xC0000000`

当前实现按这组参数初始化 LTDC，不再使用之前试错阶段的 `41/2/2/10/2/2` 屏参。

## 当前实现策略

当前 `display_lcd` 实现采用最小联调路径：

1. 初始化 `PB5` 背光 GPIO，默认先关背光
2. 初始化 `SDRAM`
3. 对 `0xC0000000` 做最小读写探针
4. 初始化 `LTDC` 和单层 `RGB565` layer
5. 写入四色测试图案
6. 点亮背光
7. 后续在状态页显示 `SET / MEAS / ERR`

失败时不让 LCD 故障拖垮主功能，串口仍持续输出：

- `sdram init failed`
- `sdram rw failed`
- `ltdc init failed`

## 已知失败模式

到本轮修正前，已经确认过两类错误来源：

1. `Bank2 + 0xD0000000`
- 会直接导致 `LCD sdram rw failed`
- 与 Apollo 板级接线不符

2. 错误的 SDRAM MSP 引脚假设
- 把 Discovery 风格的 `PB5/PB6` FMC 管脚带进来后，会干扰 Apollo 实板行为
- 还可能连带影响主业务引脚和测量稳定性

本轮修正后，如果 `SDRAM` 探针通过但仍未正常显示，后续问题范围应收缩为：

- LTDC 同步极性
- LTDC 时钟
- 背光时序
- RGB 数据线或面板连接状态

不再优先怀疑主业务的 `UART/PWM/MEAS` 链路。

## 实板验证步骤

1. 构建并烧录当前固件
2. 接回环线：`PB6(TIM4_CH1) -> PA7(TIM3_CH2)`
3. 打开串口：
   - `python3 tools/serial_monitor.py --port /dev/ttyUSB0 --baud 115200`
4. 先观察上电后的 `LCD` 状态：
   - 目标：不再出现 `LCD sdram rw failed`
5. 再验证 3 组参数：
   - 默认 `1000/50`
   - `freq 2000` + `duty 30`
   - `freq 5000` + `duty 70`
6. 拔掉回环线，确认 `MEAS no-signal`

## 当前结论

当前项目的 M6 主线已经从“猜测性 LCD 接入”收敛到“Apollo F429 手册真值驱动的 LCD 联调”。后续实板排障只需要围绕这套固定基线继续推进，不应再切回 `Bank2 + 0xD0000000` 或其他未证实的板级假设。
