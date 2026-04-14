#!/usr/bin/env python3
import argparse
import serial


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        print(f"Monitoring {args.port} @ {args.baud}")
        while True:
            data = ser.read(256)
            if data:
                print(data.decode(errors="replace"), end="")


if __name__ == "__main__":
    main()
