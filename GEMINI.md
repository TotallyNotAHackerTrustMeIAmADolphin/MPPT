# openMPPT Controller - STM32F072 Project

## Project Overview
This project is an embedded firmware for the **openMPPT v1.1** solar charge controller. It is built for the **STM32F072RBT6** MCU using the **STM32Cube** framework. The system manages DC-DC power conversion using high-frequency PWM and monitors system status through various analog sensors.

### Key Features
- **Multi-Algorithm MPPT**: Supports both **Incremental Conductance (IncCond)** for high-speed tracking and classic **Perturb and Observe (P&O)**.
- **Pre-Charge Duty Matching**: Dynamically calculates equilibrium duty cycle based on $V_{in}/V_{out}$ ratio before engaging the power stage to prevent battery backflow faults.
- **Voltage/Current Bound Sweeping**: Global power sweep terminates autonomously upon hitting soft device limits, preventing hardware overvoltage faults.
- **Multi-Mode Operation**: Selectable modes via dashboard (Solar MPPT, E-Bike Bidirectional, Power Supply CV/CC).
- **Bidirectional E-Bike Mode**: Supports motor driving (boost) and regenerative braking (buck) with seamless PI transitions.
- **Unified Multi-Variable Control**: Single control loop manages voltage, current, and regen limits simultaneously using a high-speed "Min-Selector" architecture.
- **High-Frequency PWM**: 100 kHz PWM frequency (`TIMER_PERIOD = 240`).
- **PWM Dithering**: Implements a 3-bit dithering table to improve duty cycle resolution.
- **Velocity PI Regulation**: Mathematically stable PI controller for CV/CC modes, eliminating ripple and state-switching oscillation.
- **Autonomous Operation**: Board boots and regulates without a serial connection (headless).
- **Semantic Fixed-Point Math**: All calculations use integers with semantic scaling (mV, mA, uW, ticks).
- **DMA-based ADC**: Samples 6 channels with Ping-Pong processing for zero-latency measurement.
- **Hardware Safety Architecture**: Mandatory hardware limits (80V Vin, 12.5V Min Vin, 20A Current) with descriptive fault reporting.
- **Dead-Band Escape Strategy**: Decouples logical duty cycle state from hardware-level PWM clamping.
- **Enhanced UI Stability**: 100ms state-hold hysteresis prevents dashboard flickering during limit-hitting events.
- **Interactive Calibration**: Structured serial command protocol (`CMD:CAL_...`) with automated safety bypasses for field calibration.
- **Dynamic Tuning**: Remote parameter optimization (`CMD:TUNE_...`) supported via Python hybrid search.
- **EEPROM Storage**: Integrated signature-checked Flash storage for persisting calibration and limits.

## Technologies & Architecture
- **Topology**: 4-Switch Synchronous Non-Inverting Buck-Boost.
- **MCU**: STM32F072RBT6 (ARM Cortex-M0 @ 48MHz).
- **Power Stage**: Four TI CSD19505KCS 80V N-Channel MOSFETs driven by two Infineon IRS21867STRPBF gate drivers. High-side drivers are powered by isolated B1212S-1W DC-DC converters for continuous high-voltage operation.
- **Sensing**: Precision zero-drift INA240A4DR current sense amplifiers. *(Note: Footprints for isolated ACS712ELCTR-30A-T hall-effect sensors exist on v1.1 as a planned future replacement for the INA240).*
- **Auxiliary Power**: XL7005A wide-input buck converters step down high panel voltage to a stable 12V rail (drivers/fans) and 3.3V rail (logic).
- **Framework**: STM32Cube HAL.
- **Build System**: PlatformIO using the custom `openmppt` board definition and `setup_cubemx_env_auto.py` automation bridge.
- **Hardware Automation**: `TIMER_PERIOD` is automatically extracted from `MPPT.ioc` during build.

## Building and Running
### Build
```bash
pio run
```

### Upload
```bash
pio run -t upload
```

### Monitor
```bash
pio device monitor -b 115200
```

## Development Conventions

### MPPT Algorithm Configuration
The active tracking algorithm can be toggled in `Core/Inc/mppt.h` using the `ACTIVE_MPPT_ALGO` macro:
- `MPPT_ALGO_INC_COND`: (Default) mathematically tracks the peak via $dI/dV$. Superior for shading/fast clouds.
- `MPPT_ALGO_P_AND_O`: Classic power-comparison sweep.

### Git Workflow & Branching Strategy
The project strictly follows a **Stable Main** workflow tailored for embedded systems hardware safety:
1.  **Stable Main:** The `main` branch MUST always compile cleanly and represent a hardware-safe, tested state. Never commit broken or experimental code directly to `main`.
2.  **Feature Isolation:** All new features, bug fixes, or hardware investigations MUST be developed on dedicated branches (`feature/<name>` or `fix/<name>`).
3.  **Merge Quality Gate:** A branch can only be merged into `main` after it:
    - Compiles cleanly for the `openmppt` target (no warnings, `-Werror` compliant).
    - Passes all native unit tests (`pio test -e native`).
    - **Requires User Hardware Approval:** The AI agent must prompt the user to verify the branch on physical hardware (or confirm simulation satisfaction) before executing the merge.
4.  **Atomic Commits:** Use Conventional Commits (`feat:`, `fix:`, `refactor:`). Commit messages must explain the *why*, especially for hardware timing or magic numbers.
5.  **Unified Codebase:** Handle hardware revisions via `platformio.ini` build environments or `#define` macros. Never use long-lived branches for different PCB versions.

### Code & Math Conventions
- **Fixed-Point Math**: NEVER use `float` or `double`. Use 32-bit millivolts (`_mV`) and milliamps (`_mA`), and 64-bit microwatts (`_uW`) for power.
- **Sign Awareness**: When implementing MPPT math, always cross-multiply (e.g., $dI \times V = -I \times dV$) to avoid division, and ensure sign handling for negative $dV$ is explicit in Incremental Conductance implementations.
- **Calibration Protocol**: Use machine-readable `CMD:CAL_...` format for serial interaction to maintain compatibility with future web frontends.
- **Safety First**: Power stage must be disabled (`POWER_PWM_Set(0)`) before writing to Flash (EEPROM).

## Key Files
- `Core/Src/main.c`: Core application logic and sensor processing.
- `Core/Src/controller.c`: Unified state machine and safety logic.
- `Core/Src/mppt.c`: Optimized P&O and Sweep algorithms.
- `Core/Src/power.c`: Velocity PI regulation and PWM management.
- `Core/Src/eeprom.c`: ST EEPROM Emulation storage layer.
- `STM32F072RBTX_FLASH.ld`: Linker script (modified to reserve last 4KB for EEPROM).
- `scripts/setup_cubemx_env_auto.py`: Automated build environment bridge.
- `scripts/tune_mppt.py`: Hybrid machine-learning auto-tuning script.
- `scripts/mppt_tool.py`: Advanced diagnostic and control suite.
- `hardware/`: Directory containing hardware design files and documentation.

## Diagnostic Tools

### MPPT Debug & Control Tool (`scripts/mppt_tool.py`)
This Python-based suite provides real-time interaction with the firmware via the USB-CDC interface. It is the primary tool for telemetry analysis and fault diagnosis.

#### Usage Modes
- **Monitoring & Control**: `python3 scripts/mppt_tool.py --monitor`
  - Streams JSON telemetry to `mppt_log.csv`.
  - Provides an interactive prompt to send raw serial commands (e.g., `RESET_FAULT`, `SET_I_MAX:5000`).
- **Autonomous Watchdog**: `python3 scripts/mppt_tool.py --watch`
  - Monitors the system until a fault occurs.
  - Automatically captures and saves a high-resolution `fault_dump_XXX.json` containing the telemetry leading up to the fault.
- **Single Command**: `python3 scripts/mppt_tool.py --cmd "CMD:RESET_FAULT"`
  - Executes a single command and exits.

### Hardware LED Diagnostics
The onboard LED (`PC10`) provides visual status and fault diagnostic codes.

- **Heartbeat (Slow Blink)**: System is in `STATE_ACTIVE` and operating normally.
- **Fault Blink Codes**: When in `STATE_FAULT`, the LED blinks a specific number of times followed by a 1-second pause:
  - **1 Pulse**: Input Over-voltage
  - **2 Pulses**: Input Under-voltage
  - **3 Pulses**: Input Over-current
  - **4 Pulses**: Output Over-voltage
  - **5 Pulses**: Output Over-current
  - **6 Pulses**: Backflow Fault
  - **7 Pulses**: Over-temperature

#### Requirements
- `pyserial` (Python 3)
- Active USB connection to the openMPPT board.
