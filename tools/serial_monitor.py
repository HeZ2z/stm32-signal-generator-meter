#!/usr/bin/env python3
"""最小串口监视工具：直接打印开发板输出，便于人工联调。"""

import argparse
import serial


def main() -> None:
    """按给定串口参数持续读取并原样输出串口数据。"""
    parser = argparse.ArgumentParser()
    # 默认串口参数与当前 Apollo 开发板的 CH340 调试链路一致。
    parser.add_argument("--port", default="/dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        print(f"Monitoring {args.port} @ {args.baud}")
        while True:
            data = ser.read(256)
            if data:
                # 联调阶段优先保证不断流输出，乱码交给 replace 容错处理。
                print(data.decode(errors="replace"), end="")


if __name__ == "__main__":
    main()
