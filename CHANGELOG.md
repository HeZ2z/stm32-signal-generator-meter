# Changelog

All notable changes to this project will be documented in this file.

The format loosely follows Keep a Changelog.

## [Unreleased]

### Added

- 双路 DAC + TIM6 波形表驱动，支持 `square / triangle / sine` 三种波形输出。
- 双通道 ADC + DMA 回采，LCD 双通道 `YT` 叠加波形页。
- 最小 `XY` 模式与完整李萨如演示链路（phase / ratio 参数，覆盖同频同相、`90°` 相移、`1:2` 频率比）。
- `display_lcd_xy_logic` 纯逻辑模块及宿主机测试。
- `DMA2D` 加速 `lcd_fill_rect`，低于阈值回退 CPU 填充。
- display 层 scene 状态机与 profiling 基础设施。
- `ui_ctrl` 宿主机测试与 HAL stub 基础设施；DAC 逻辑边界测试。
- 失败语义收口：`FREQ OUT OF RANGE` 与 `DAC RECONFIG FAIL` 区分提示，`OUTPUT STOPPED / DAC INACTIVE` 状态显示。

### Changed

- `stop_dual_dma()` 重构为唯一停机 owner，`apply()` 失败统一置 `dac_status.active=false` 由 display 层消费。
- scope 与 XY 显示逻辑重构为 `plot_refresh / info_refresh` 分离结构。
- HAL/UART 相关代码隔离到 `#ifndef HOST_TEST` 保护块。

### Pending

- M11 实板三场景验证（同频同相、同频 `90°` 相移、`1:2` 频率比）与文档最终收口。

## [0.1.0-bootstrap] - 2026-04-14

### Added

- Reworked repository `README.md` into a project-homepage style document.
- Added baseline repository files: `.gitignore`, `.editorconfig`, `.gitattributes`, `LICENSE`, `CONTRIBUTING.md`.
- Added initial project documentation for architecture, roadmap, pin planning, verification, and decisions.
- Added `src/`, `include/`, `cmake/`, and `tools/` directory guides to make the starter layout explicit.
