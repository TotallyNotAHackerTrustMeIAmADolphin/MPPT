# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Firmware for **openMPPT**, a solar MPPT (Maximum Power Point Tracking) charge controller built around an **STM32F072RBT6** (ARM Cortex-M0 @ 48MHz, no FPU) using the **STM32Cube HAL** framework and **PlatformIO**. The board is a 4-switch synchronous non-inverting buck-boost converter that also supports bidirectional (e-bike motor) and bench power-supply (CV/CC) modes, with a Nokia 5110 LCD for local telemetry and a USB-CDC JSON serial protocol for remote telemetry/control. The repo also contains the KiCad hardware design (`hardware/`) and a Web Serial dashboard (`docs/`).

## Build, Test, and Run Commands

```bash
# Build firmware for the real board
pio run

# Upload (DFU by default; Black Magic Probe available via platformio.ini)
pio run -t upload

# Serial monitor (115200 baud)
pio device monitor -b 115200

# Run native unit tests (host-compiled, no hardware needed)
pio test -e native

# Run a single native test file (Unity test runner, filters by test case name substring)
pio test -e native -f test_mppt --filter test_mppt_increase_power
```

- The `native` PlatformIO environment compiles `Core/Src/mppt.c` (and `controller.c` via test includes) directly against Unity, with hand-written mocks for HAL/sensors/power/comms — see `test/test_controller/test_controller.c` and `test/test_mppt/test_mppt.c` for the mocking pattern used when adding new tests.
- `-Werror -Wall` is set globally in `platformio.ini`; the firmware must compile warning-free.
- `TIMER_PERIOD` and other PWM constants are auto-extracted from `MPPT.ioc` at build time by `scripts/pre:setup_cubemx_env_auto.py` — don't hardcode them separately.

### Diagnostic / tuning scripts (Python, `pyserial`)

```bash
pip install pyserial

python scripts/mppt_tool.py --monitor          # live telemetry -> mppt_log.csv, interactive command prompt
python scripts/mppt_tool.py --watch            # watchdog; auto-saves fault_dump_XXX.json on fault
python scripts/mppt_tool.py --cmd "CMD:RESET_FAULT"
python scripts/tune_mppt.py --port /dev/ttyACM0  # hybrid (random search + local fine-tune) MPPT parameter tuning
```

## Architecture

### Control flow / module layering

`main.c` runs a superloop: ADC sampling → `sensors.c` (raw ADC → physical units via ping-pong DMA buffers) → `controller.c` (state machine) → `mppt.c` / `power.c` (duty cycle decision) → PWM output, plus a decoupled 10Hz LCD refresh and 100ms telemetry emit. Everything is fixed-point integer math (see below) — there is no RTOS; timing separation between the ~1.5kHz control loop and low-rate UI/telemetry is done by interval counters in the main loop, not preemption.

- **`controller.c`** — the unified state machine (`SystemState_t`: IDLE → SWEEPING → ACTIVE → FAULT → RECOVERY) and safety/limit logic. Uses a single "min-selector" architecture: one control loop simultaneously enforces `SoftLimit_t` (V_OUT_MAX, I_OUT_MAX, V_IN_MIN, V_IN_MAX, I_OUT_MIN/backflow) rather than separate mode-specific loops, so CV/CC/MPPT/reverse-flow all fall out of the same code path.
- **`mppt.c`** — the perturb-and-observe / incremental-conductance tracking algorithms and the global sweep. Algorithm choice is compile-time via `ACTIVE_MPPT_ALGO` in `Core/Inc/mppt.h` (`MPPT_ALGO_INC_COND` default, or `MPPT_ALGO_P_AND_O`).
- **`power.c`** — velocity-form PI regulation (not positional PI — avoids double-integration windup) and PWM/dithering management. Also computes pre-charge/voltage-match duty (`POWER_CalculateVoltageMatchDuty`) so the converter starts near equilibrium duty before closing the loop, preventing backflow-fault-on-startup.
- **`sensors.c`** — ADC ping-pong DMA processing, 6 channels (Vin, Vout, Iin, Iout, MOSFET temp, MCU temp).
- **`eeprom.c`** — ST EEPROM-emulation Flash storage for calibration/limits, guarded by a signature check (`0xABCD`) for safe defaults on first flash. PWM must be disabled (`POWER_PWM_Set(0)`) before any Flash write.
- **`comms.c`** — USB-CDC JSON telemetry + `CMD:...` command parsing (calibration `CMD:CAL_...`, tuning `CMD:TUNE_...`, etc.).
- **`settings.c`** — runtime limits/calibration accessors backing the above (`SETTINGS_GetLimits`, `SETTINGS_IsCalibrating`, ...).
- **`system_types.h`** / **`system_config.h`** — shared structs (`Measurements_t`, `DeviceLimits_t`, `PID_t`, `Calibration_t`) and system-wide tunables (timing intervals, hardware safety limits, hysteresis). Read these first when touching control logic — they're the shared vocabulary across every module above.

### Fixed-point math convention (STM32F0 has no FPU)

- **Never use `float`/`double`.** All quantities are integers with a unit suffix baked into the variable name: `_mV`, `_mA`, `_uW`/`_mW` (power; 64-bit for microwatts), `_ticks` (PWM). Always multiply before dividing in scaling logic to preserve precision.
- MPPT math cross-multiplies to avoid division (e.g. `dI * V` vs `-I * dV`) and explicitly handles the sign of `dV` in incremental-conductance code.

### PWM / sensing hardware details

- 100kHz PWM via TIM1, with a 3-bit dither table (8 cycles) raising effective resolution from 240 to 1920 steps; DMA auto-updates duty per dither cycle.
- ADC uses circular DMA with double buffering (ping-pong) across 6 channels — CPU processes one half while DMA fills the other.
- Nokia 5110 (PCD8544) LCD on SPI1: CLK=PB3, DIN=PB5, DC=PB13, CE=PB14, RST=PB12. Non-blocking 10Hz refresh, isolated from the control loop.
- Onboard LED (PC10) is the diagnostic heartbeat (slow blink = STATE_ACTIVE) and fault code indicator (N pulses + 1s pause; 1=Input OV, 2=Input UV, 3=Input OC, 4=Output OV, 5=Output OC, 6=Backflow, 7=Overtemp) — see `FaultReason_t` in `system_types.h`.

### Hardware safety limits (do not relax without hardware justification)

`Core/Inc/system_config.h`: Vin 80V max / 12.5V min, Vout 80V max, Iin/Iout 20A max, MCU temp 85°C max.

## Git Workflow (Stable-Main, PR-based)

1. `main` must always compile cleanly (`-Werror`) and represent a hardware-safe, tested state — never commit broken/experimental code directly to `main`, and never push or merge directly to `main`.
2. New features/fixes go on `feature/<name>` or `fix/<name>` branches (firmware/docs/tooling) or `hardware/<name>` branches (anything under `hardware/`), pushed to `origin`, and merged into `main` only via a Pull Request.
3. Every PR must compile clean for the `openmppt` target and pass `pio test -e native` before it's mergeable; note what was tested (native suite, hardware bench, simulation) in the PR description.
4. **The user must confirm hardware verification (or explicit simulation sign-off) before a PR touching control/safety logic is merged** — do not merge on your own judgment alone, even if CI is green.
5. Use Conventional Commits (`feat:`, `fix:`, `refactor:`, `chore:`, `docs:`) for individual commits; explain *why* in the message, especially for hardware timing constants or magic numbers. Keep unrelated concerns (e.g. tooling cleanup vs. firmware logic) as separate commits within the PR so history stays reviewable.
6. Squash-merge once approved and checks pass (keeps `main` linear); delete the feature branch after merge.
7. Handle hardware revisions via `platformio.ini` build environments or `#define` macros, not long-lived per-revision branches.

## Dashboard (`docs/`)

Web Serial dashboard served at the GitHub Pages link in README.md; talks to the board via USB CDC JSON telemetry. **Always bump the version number in the `<h1>` tag of `docs/index.html` when changing any file under `docs/`.**

## Hardware (`hardware/`)

KiCad source lives in `hardware/KiCad/`; component rationale and E24/E96 resistor derivations are in `hardware/CALCULATIONS.typ` (Typst — compile with `typst compile hardware/CALCULATIONS.typ` or read the source directly); current component/pinout mapping ("Hardware Universe") and engineering mandates are in `hardware/STANDARDS.md`; phase-by-phase v1.3 hardware plan is in `hardware/ROADMAP_V1.3.md`.

- High-power traces (VIN, VOUT, GND, VS_A, VS_B) must be sized for 20A continuous (3.5mm standard width).
- Prefer the PCM-JLCPCB library for new parts.
- Any new component mapping or pinout must be added to the "Hardware Universe" table in `hardware/STANDARDS.md` immediately.
- No hardware changes merge to `main` without a clean DRC/ERC report (`kicad-cli`, path noted in `hardware/STANDARDS.md`).
