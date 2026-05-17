# MPPT Controller - STM32F072 Project

## Project Overview
This project is an embedded firmware for a Maximum Power Point Tracking (MPPT) solar charge controller. It is built for the **STM32F072RBT6** MCU (Nucleo-F072RB board) using the **STM32Cube** framework. The system manages DC-DC power conversion using high-frequency PWM and monitors system status through various analog sensors.

### Key Features
- **High-Frequency PWM**: 100 kHz PWM frequency.
- **PWM Dithering**: Implements a 3-bit dithering table to improve duty cycle resolution (0-1920 ticks).
- **Semantic Fixed-Point Math**: All calculations use integers with semantic scaling (mV, mA, uW, ticks) for maximum performance on Cortex-M0.
- **DMA-based ADC**: Samples 6 channels with **Ping-Pong buffering** for zero-latency, race-condition-free processing.
- **PID Control**: Custom integer-based PID implementation for voltage/current regulation.
- **USB Communication**: USB CDC (Virtual COM Port) for telemetry and debugging.

## Technologies & Architecture
- **MCU**: STM32F072RBT6 (ARM Cortex-M0)
- **Framework**: STM32Cube HAL
- **Build System**: PlatformIO with a custom script (`setup_cubemx_env_auto.py`).
- **Hardware Automation**: `TIMER_PERIOD` is automatically extracted from `MPPT.ioc` during build and injected as a compiler define.

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
- **CubeMX Compatibility**: Always place manual code within `/* USER CODE BEGIN */` and `/* USER CODE END */` blocks.
- **Hardware Scaling**: Use macros derived from `TIMER_PERIOD` and `DITHER_TABLE_SIZE` for all PWM and control loop limits.

## Key Files
- `platformio.ini`: Project configuration.
- `MPPT.ioc`: STM32CubeMX configuration file.
- `Core/Src/main.c`: Core application logic and sensor processing.
- `setup_cubemx_env_auto.py`: Automated build environment bridge.
- `STM32F072RBTX_FLASH.ld`: Linker script.
