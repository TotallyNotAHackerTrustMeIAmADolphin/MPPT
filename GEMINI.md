# MPPT Controller - STM32F072 Project

## Project Overview
This project is an embedded firmware for a Maximum Power Point Tracking (MPPT) solar charge controller. It is built for the **STM32F072RBT6** MCU (Nucleo-F072RB board) using the **STM32Cube** framework. The system manages DC-DC power conversion using high-frequency PWM and monitors system status through various analog sensors.

### Key Features
- **High-Frequency PWM**: 100 kHz PWM frequency (`TIMER_PERIOD = 240`).
- **PWM Dithering**: Implements a 3-bit dithering table to improve duty cycle resolution.
- **Semantic Fixed-Point Math**: All calculations use integers with semantic scaling (mV, mA, uW, ticks) for maximum performance on Cortex-M0.
- **DMA-based ADC**: Samples 6 channels with Ping-Pong buffering for zero-latency processing.
- **PID Control**: Custom integer-based PID implementation for voltage/current regulation.
- **USB Communication**: USB CDC (Virtual COM Port) for telemetry and debugging.

## Technologies & Architecture
- **MCU**: STM32F072RBT6 (ARM Cortex-M0)
- **Framework**: STM32Cube HAL
- **Build System**: PlatformIO with a custom script (`setup_cubemx_env_auto.py`) to bridge with STM32CubeIDE/CubeMX.
- **Main Logic**: Located in `Core/Src/main.c`.
- **USB Interface**: Managed in `USB_DEVICE/` and `usbd_cdc_if.c`.

## Building and Running
The project uses PlatformIO for development.

### Build
```bash
pio run
```

### Upload
The project is configured to use **DFU** for uploading.
```bash
pio run -t upload
```

### Monitor
Default serial monitor settings: 115200 baud.
```bash
pio device monitor
```

## Development Conventions
- **CubeMX Compatibility**: Always place manual code within `/* USER CODE BEGIN */` and `/* USER CODE END */` blocks.
- **Fixed-Point Arithmetic**: NEVER use `float` or `double`. Use 32-bit millivolts (`_mV`) and milliamps (`_mA`), and 64-bit microwatts (`_uW`) for power.
- **PWM Logic**: PWM is managed in raw dithered ticks (0-1920 for 0-100%).
- **Telemetry**: Use integer format specifiers (`%ld`, `%lld`) in `printf`.
- **Hardware Abstraction**: Uses HAL drivers for peripherals.

## Key Files
- `platformio.ini`: Project configuration, includes specific toolchain and upload settings.
- `MPPT.ioc`: STM32CubeMX configuration file.
- `Core/Src/main.c`: Core application logic, PID loops, and sensor processing.
- `Core/Inc/pidautotuner.h`: Interface for the PID auto-tuning library.
- `setup_cubemx_env_auto.py`: Custom script to sync CubeMX generated files with the PlatformIO environment.
