# openMPPT Controller - STM32F072 Project

## Project Overview
This project is an embedded firmware for the **openMPPT v1.1** solar charge controller. It is built for the **STM32F072RBT6** MCU using the **STM32Cube** framework. The system manages DC-DC power conversion using high-frequency PWM and monitors system status through various analog sensors.

### Key Features
- **Custom Hardware**: Optimized for the openMPPT v1.1 board (Cortex-M0 based).
- **High-Frequency PWM**: 100 kHz PWM frequency (`TIMER_PERIOD = 240`).
- **PWM Dithering**: Implements a 3-bit dithering table to improve duty cycle resolution.
- **Velocity PI Regulation**: mathematically stable PI controller for CV/CC modes, eliminating ripple.
- **Autonomous Operation**: Board boots and regulates without a serial connection (headless).
- **Semantic Fixed-Point Math**: All calculations use integers with semantic scaling (mV, mA, uW, ticks).
- **DMA-based ADC**: Samples 6 channels with Ping-Pong processing for zero-latency measurement.
- **Hardware Safety Architecture**: Mandatory hardware limits (80V Vin, 12.5V Min Vin, 20A Current) with descriptive fault reporting.
- **Dead-Band Escape Strategy**: Decouples logical duty cycle state from hardware-level PWM clamping. This allows small MPPT steps to accumulate and "walk" out of the 100% passthrough dead-band without losing state.
- **Interactive Calibration**: Structured serial command protocol (`CMD:CAL_...`) for field calibration.
- **Dynamic Tuning**: Remote parameter optimization (`CMD:TUNE_...`) supported via Python hybrid search.
- **EEPROM Storage**: Integrated signature-checked Flash storage for persisting calibration and limits.

## Technologies & Architecture
- **MCU**: STM32F072RBT6 (ARM Cortex-M0)
- **Framework**: STM32Cube HAL
- **Build System**: PlatformIO with a custom automation script (`scripts/setup_cubemx_env_auto.py`).
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
- **Fixed-Point Math**: NEVER use `float` or `double`. Use 32-bit millivolts (`_mV`) and milliamps (`_mA`), and 64-bit microwatts (`_uW`) for power.
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
- `hardware/`: Directory containing hardware design files and documentation.
