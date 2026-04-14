# Changelog

All notable changes to this project will be documented in this file.

The format loosely follows Keep a Changelog.

## [Unreleased]

### Added

- Stable main firmware for `UART + PWM + serial command + LED heartbeat`.
- `STM32CubeF4` vendor dependency pinned as a proper git submodule.
- Host-side regression tests for serial command parsing and `PWM` parameter logic.
- Standalone `CTest` entry point for host-side tests under `tests/`.
- GitHub Actions CI workflow for host-side tests and firmware cross-builds.

### Changed

- Mainline scope narrowed to the stable demo firmware path for one-week delivery.
- Documentation updated to reflect current progress at `M3 complete`, `M5 partial`, `M4 paused`.
- Project verification records updated with serial bring-up, firmware build, and host-test evidence.

### Removed

- Unstable measurement path from the main firmware branch.

## [0.1.0-bootstrap] - 2026-04-14

### Added

- Reworked repository `README.md` into a project-homepage style document.
- Added baseline repository files: `.gitignore`, `.editorconfig`, `.gitattributes`, `LICENSE`, `CONTRIBUTING.md`.
- Added initial project documentation for architecture, roadmap, pin planning, verification, and decisions.
- Added `src/`, `include/`, `cmake/`, and `tools/` directory guides to make the starter layout explicit.
