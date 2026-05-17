# MPPT Controller - STM32F072 Project

## Project Overview
This project is an embedded firmware for a Maximum Power Point Tracking (MPPT) solar charge controller. It is built for the **STM32F072RBT6** MCU (Nucleo-F072RB board) using the **STM32Cube** framework. The system manages DC-DC power conversion using high-frequency PWM and monitors system status through various analog sensors.

### Key Features
- **High-Frequency PWM**: 100 kHz PWM frequency (`TIMER_PERIOD = 240`).
- **PWM Dithering**: Implements a 3-bit dithering table to improve duty cycle resolution.
- **Semantic Fixed-Point Math**: All calculations use integers with semantic scaling (mV, mA, uW, ticks) for maximum performance on Cortex-M0.
- **DMA-based ADC**: Samples 6 channels with Ping-Pong (Half-Buffer/Full-Buffer) processing for zero-latency measurement.
- **Interactive Calibration**: Structured serial command protocol (`CMD:CAL_...`) for field calibration via terminal or Web Serial API.
- **EEPROM Storage**: Integrated ST EEPROM Emulation for persisting calibration constants in Flash (last 4KB reserved).
- **Supply-Aware MPPT Sweep**: Automatically scans the power curve every 300s. Includes intelligent early-termination logic that stops the sweep if the source voltage sags below a safety floor (14V).

## Technologies & Architecture
- **MCU**: STM32F072RBT6 (ARM Cortex-M0)
- **Framework**: STM32Cube HAL
- **Build System**: PlatformIO with a custom script (`setup_cubemx_env_auto.py`).
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

## Key Files
- `Core/Src/main.c`: Core application logic and sensor processing.
- `Core/Src/eeprom.c`: ST EEPROM Emulation storage layer.
- `STM32F072RBTX_FLASH.ld`: Linker script (modified to reserve last 4KB for EEPROM).
- `setup_cubemx_env_auto.py`: Automated build environment bridge.
