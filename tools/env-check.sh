#!/usr/bin/env bash
set -euo pipefail

echo "arm-none-eabi-gcc: $(command -v arm-none-eabi-gcc || echo missing)"
echo "cmake: $(command -v cmake || echo missing)"
echo "ninja: $(command -v ninja || echo missing)"
echo "openocd: $(command -v openocd || echo missing)"
echo "python3: $(command -v python3 || echo missing)"
echo "serial nodes:"
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || true
