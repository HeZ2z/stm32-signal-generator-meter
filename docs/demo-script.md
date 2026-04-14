# Demo Script

用于演示和答辩时按固定顺序展示系统能力，避免现场临时操作失误。

## 演示前准备

- 固件已烧录到开发板
- 开发板正常从 Flash 启动
- 杜邦线连接 `PB6(TIM4_CH1)` 到 `PB5(TIM3_CH2)`
- 串口连接到 `/dev/ttyUSB0`
- 打开串口监视：

```bash
sudo python3 tools/serial_monitor.py --port /dev/ttyUSB0 --baud 115200
```

## 演示步骤

1. 上电或复位后，说明系统已经完成 `PWM 输出 + 输入测量 + UART 输出` 闭环。
2. 展示默认状态：

```text
SET freq=1000Hz duty=50% | MEAS freq=1001Hz period=999us duty=50% | ERR df=1Hz dt=-1us dd=0%
```

3. 发送：

```text
freq 2000
duty 30
```

说明系统已经从默认参数切换到新的目标输出。

4. 展示 `2000/30` 的实测结果：

```text
SET freq=2000Hz duty=30% | MEAS freq=2004Hz period=499us duty=30% | ERR df=4Hz dt=-1us dd=0%
```

5. 再发送：

```text
freq 5000
duty 70
```

6. 展示 `5000/70` 的实测结果：

```text
SET freq=5000Hz duty=70% | MEAS freq=5025Hz period=199us duty=70% | ERR df=25Hz dt=-1us dd=0%
```

7. 拔掉 `PB6 -> PB5` 回环线，展示异常场景：

```text
SET freq=5000Hz duty=70% | MEAS no-signal | check PB6->PB5 loopback
```

8. 重新接回回环线，说明系统可恢复到正常测量状态。

## 演示时建议说明

- `SET` 表示设定值，`MEAS` 表示实测值，`ERR` 表示误差摘要。
- 当前误差主要来自 `1 MHz` 计数时钟的 `1 us` 量化分辨率。
- 断线后系统不会继续显示旧测量值，而是明确提示 `no-signal`。
- 这套方案不依赖外部信号源，只需要单板和一根杜邦线即可完成完整演示。

## 失败回退方案

- 若串口没有输出，先检查是否正常从 Flash 启动、波特率是否为 `115200`。
- 若只有 `SET` 没有有效 `MEAS`，优先检查 `PB6 -> PB5` 杜邦线。
- 若出现 `ERR unknown command`，重新输入标准命令：`help`、`status`、`freq <hz>`、`duty <1-99>`。
