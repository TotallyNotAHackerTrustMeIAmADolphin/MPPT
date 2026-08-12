# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Embedded firmware (C, STM32Cube HAL) for **openMPPT**, a solar MPPT charge controller built on an
**STM32F072RBT6** (ARM Cortex-M0 @ 48MHz, no FPU). Topology is a 4-switch synchronous non-inverting
buck-boost converter that can also run as a bidirectional e-bike motor controller or a bench CV/CC
power supply, selectable at runtime via `DeviceLimits_t.mode`. The board is headless — it boots and
regulates without a serial connection; USB-CDC is used only for telemetry/config, and an integrated
Nokia 5110 (PCD8544) LCD provides on-device readout.

There is also a `docs/` static web dashboard (Web Serial API) that talks to the firmware over USB-CDC,
and a set of Python scripts under `scripts/` for calibration, tuning, and diagnostics.

## Build, Test, Upload

Build system is **PlatformIO**, driven by `platformio.ini` plus the custom `openmppt` board definition
in `boards/openmppt.json`.

```bash
# Build firmware for the target (env:openmppt)
pio run

# Upload via Black Magic Probe (default upload_protocol; no NRST pin needed)
pio run -t upload

# Serial monitor (115200 baud)
pio device monitor -b 115200

# Run native unit tests (env:native, runs on host, not the MCU)
pio test -e native
```

Notes on the build:
- `env:openmppt` uses `framework = stm32cube` and a `pre:` extra script,
  `scripts/setup_cubemx_env_auto.py`, which parses the Eclipse/CubeIDE `.project`/`.cproject` files to
  derive source/include dirs and MCU compiler flags, and also scrapes `TIM1.Period=` out of `MPPT.ioc`
  at build time to inject `-DTIMER_PERIOD=<n>`. If you change the PWM period in CubeMX/`MPPT.ioc`, no
  manual firmware constant needs updating — it's picked up automatically. `lib/STLinkedResources/` is a
  symlinked HAL/USB source tree managed by this script.
- Global `build_flags` are `-Werror -Wall` for every environment — treat any new warning as a build
  failure.
- `env:native` builds `Core/Src/mppt.c` only (`build_src_filter = +<Core/Src/mppt.c> -<Core/Src/main.c>`)
  against `test/test_mppt`, with hand-written mocks for `POWER_PWM_*`. It compiles the algorithm's `.c`
  file directly into the test binary (`#include "../../Core/Src/mppt.c"`) rather than linking — the
  fixed-point MPPT/IncCond math is the part of this codebase most amenable to fast host-side testing.
  `test/test_controller` and `test/test_ebike` also exist with their own `SENSORS_/SETTINGS_` mocks but
  are not included by the current `build_src_filter`; wire up a matching filter (or per-test
  `test_build_src_filter`) before relying on `pio test -e native` to run them.
- Per `GEMINI.md`'s branching policy, `main` must always compile cleanly (`-Werror`) and pass
  `pio test -e native` before merging; feature work belongs on `feature/<name>` or `fix/<name>` branches.

## Architecture

### Control flow (`Core/Src/main.c`)
`main()` does one-time init (`SETTINGS_Init` → `SENSORS_Init` → `POWER_Init` → `CONTROLLER_Init` →
`COMMS_Init`, LCD init) then runs a superloop:
1. `COMMS_HandleCommands()` — drain and dispatch any pending USB-CDC command each iteration.
2. When the ADC ping-pong buffer half is ready (`SENSORS_IsBufferReady()`): `SENSORS_Process()` then
   `CONTROLLER_UpdateHighRate()` — this is the high-rate (~1.5 kHz) safety + regulation path.
3. `CONTROLLER_Task()` — lower-rate housekeeping: 10 Hz telemetry, fault LED blink codes, sweep
   stepping/scheduling.
4. A 10 Hz, non-blocking LCD redraw of Vin/Vout/Pout/state.

IWDG is refreshed once per loop; anything that blocks the superloop for >~1s will reset the board.

### Unified Min-Selector control (`Core/Src/controller.c`)
The regulator is **not** a set of discrete state-specific PID loops. Every control objective that could
be active at once — MPPT tracking, output CV (`vOutMax_mV`), output CC (`iOutMax_mA`), reverse/backflow
CC floor (`iOutMin_mA`), input CV ceiling for regen (`vInMax_mV`) — independently computes its own
desired duty-cycle delta each tick in `CONTROLLER_UpdateHighRate()`. The **minimum** delta wins
(min-selector), and whichever objective won is recorded as `activeSoftLimit` for telemetry/UI (`CV`,
`CC`, `REVERSE`, `VIN_LIM`, `BROWNOUT`, or plain `MPPT`). The winning delta is accumulated into a 64-bit
fixed-point integrator (`globalDutyIntegral`, ticks × 1000) — this is the "Velocity PI" scheme described
in `docs/tuning_guide.md`: it eliminates double-integration windup/instability that a naive per-mode PID
would have when switching modes. `GAIN_KP`/`GAIN_KI` in `controller.c` are the tuned gains; see
`docs/tuning_guide.md` for their meaning and lab-bench tuning procedure.

State machine (`SystemState_t`): `IDLE → SWEEPING → ACTIVE ⇄ FAULT/RECOVERY`. `SWEEPING` (MPPT mode
only) runs a slow global sweep (`MPPT_RunSweep`, ~15s) to find a good starting point before handing off
to `ACTIVE`. Hard hardware faults (over-voltage/current, backflow, over-temp) are detected in
`CONTROLLER_UpdateHighRate()` against constants in `system_config.h`; some trip in a single frame
(input/output OV, input UV, backflow), others need `FAULT_THRESHOLD_FRAMES` consecutive frames to avoid
noise trips. `handleFaultBlink()` encodes the fault reason as an LED blink count (see fault codes in
`GEMINI.md`).

### MPPT algorithms (`Core/Src/mppt.c`, `Core/Inc/mppt.h`)
Two interchangeable algorithms, selected at compile time via `ACTIVE_MPPT_ALGO` in `mppt.h`
(`MPPT_ALGO_INC_COND` is the current default, `MPPT_ALGO_P_AND_O` the classic alternative). Both return
a duty-cycle **delta** (not an absolute value) that feeds into the min-selector. Step size, power
threshold, and tracking interval are runtime-tunable (`MPPT_SetStepSize`/`SetThreshold`/`SetInterval`)
via the serial `CMD:TUNE_*` protocol, which is what `scripts/tune_mppt.py` drives.

### Fixed-point math discipline
This MCU has no FPU — **never use `float`/`double`**. All physical quantities are scaled integers:
millivolts (`_mV`), milliamps (`_mA`), microwatts (`_uW`, 64-bit), and raw dithered PWM ticks (`_ticks`).
Always multiply before dividing to preserve precision. When deriving MPPT deltas, cross-multiply
(`dI·V` vs `-I·dV`) instead of dividing, and keep sign handling for negative `dV` explicit.

### PWM / power stage (`Core/Src/power.c`, `Core/Src/tim.c`)
100 kHz PWM on TIM1 with a 3-bit dither table (`DITHER_TABLE_SIZE = 8`) driven by DMA, raising effective
resolution from `TIMER_PERIOD` steps to `TIMER_PERIOD * 8` ticks. `POWER_CalculateVoltageMatchDuty()`
pre-charges the duty cycle to the Vin/Vout equilibrium point before the power stage is enabled
(`POWER_Start()`), avoiding a backflow fault on startup/state-entry. `POWER_PWM_Set(0)` (full shutdown)
must precede any EEPROM/Flash write — see `SETTINGS_Save*`/`eeprom.c`.

### Sensing (`Core/Src/adc.c`, `Core/Src/sensors.c`)
Circular DMA continuously samples 6 channels (Vin, Vout, Iin, Iout, MOSFET temp, MCU temp) into a
ping-pong buffer (`ADC_BUF_LEN = ADC_CHANNEL_COUNT * ADC_SAMPLE_COUNT`). `SENSORS_IsBufferReady()`
returns which half just filled; `main.c` computes the correct offset and calls `SENSORS_Process()` on
that half while DMA fills the other — never touch the "hot" half from the main loop.

### Settings / calibration / EEPROM (`Core/Src/settings.c`, `Core/Src/eeprom.c`)
`DeviceLimits_t` (mode + soft limits) and `Calibration_t` (ADC raw↔real linear cal points per channel)
are persisted via ST's EEPROM-emulation-over-Flash layer, guarded by a `0xABCD` signature so first-flash
boots fall back to safe defaults. `STM32F072RBTX_FLASH.ld` is modified to reserve the last 4KB for this.
Calibration is interactive over serial (`CMD:CAL_ENTER/CAL_MODE_I|V/CAL_I_LOW|HIGH/CAL_V_LOW|HIGH/
CAL_SAVE/CAL_EXIT`), driving one high-side switch at a time while the host script reads raw ADC sums.

### Serial protocol (`Core/Src/comms.c`)
Command line protocol over USB-CDC, newline-terminated, `CMD:<NAME>[:<arg>]` in, `ACK:<NAME>_OK[:<val>]`
or a JSON object out. Telemetry (`{"type":"telemetry",...}`) is pushed at 10 Hz from `CONTROLLER_Task()`;
`{"type":"limits",...}` and `{"type":"cal_raw",...}` are query/calibration responses. If you add a new
`CMD:`, add the matching branch in `COMMS_HandleCommands()` and, if it's a persistent setting, wire it
into `SETTINGS_Save*`. The web dashboard (`docs/app.js`) and `scripts/mppt_tool.py`/`tune_mppt.py` are
both clients of this same protocol — keep it a stable, machine-readable contract when changing it.

### Display (`Core/Src/lcd_pcd8544.c`)
Nokia 5110 over SPI1, wired per the pin table in `GEMINI.md` (CLK=PB3, DIN=PB5, DC=PB13, CE=PB14,
RST=PB12). Redraws happen from the main loop's non-blocking 10 Hz task — keep any LCD-touching code off
the high-rate control path.

## Conventions

- **Naming**: always suffix variables with their unit (`currentIn_mA`, `voltageOut_mV`,
  `powerOut_uW`, `dutyCycle_ticks`).
- **No floats.** Integer/fixed-point only, multiply-before-divide.
- **Safety ordering**: disable the power stage (`POWER_PWM_Set(0)`) before any Flash/EEPROM write.
- **Dashboard versioning**: bump the version string in the `<h1>` of `docs/index.html` whenever any file
  under `docs/` changes.
- Conventional Commits (`feat:`, `fix:`, `refactor:`, `docs:`) with commit messages explaining *why*,
  especially for hardware timing constants or other "magic numbers."
