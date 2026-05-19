# MPPT Controller - STM32F072 Firmware

A professional-grade firmware for a Maximum Power Point Tracking (MPPT) solar charge controller, optimized for the **ARM Cortex-M0** architecture. This project implements high-frequency power conversion with advanced sensing, robust safety, and optimized regulation logic.

---

## 🚀 Key Technical Features

### 1. High-Frequency PWM with Resolution Dithering
- **Frequency:** 100 kHz switching frequency for high power density.
- **Dithering:** Implements a **3-bit dithering table** (8 cycles). This effectively increases the PWM resolution from 240 steps to **1920 steps**, allowing for ultra-fine-grained voltage and current control.
- **Hardware Integration:** Managed via TIM1 with DMA-based automated duty cycle updates.

### 2. High-Performance Sensing (Ping-Pong ADC)
- **Zero-Latency DMA:** Uses circular DMA to sample 6 analog channels continuously.
- **Double Buffering:** Implements industry-standard **Ping-Pong buffering**. The CPU processes one half of the sample buffer while the DMA fills the other half, eliminating data tearing and race conditions.
- **Channels:** Input Voltage, Output Voltage, Input Current, Output Current, MOSFET Temperature, and MCU Internal Temperature.

### 3. Optimized Math Engine (Semantic Fixed-Point)
- **No FPU overhead:** Designed specifically for the STM32F0 (which lacks a hardware Floating Point Unit). All calculations use high-speed 32-bit and 64-bit integer arithmetic.
- **Unit Precision:** 
  - **Voltages:** Millivolts (`_mV`)
  - **Currents:** Milliamps (`_mA`)
  - **Power:** Microwatts (`_uW`)
  - **PWM:** Raw Dithered Ticks (`_ticks`)

### 4. Advanced Control & Safety Logic
- **Velocity PI Regulation:** Implements a mathematically stable **Velocity PI algorithm** for Constant Voltage (CV) and Constant Current (CC) modes. This eliminates double-integration instability and provides rock-solid, ripple-free regulation.
- **Limit-Aware MPPT (P&O):** A refined Perturb & Observe algorithm with **power accumulation logic**. It accurately tracks the maximum power point on gentle slopes and intelligently "parks" the duty cycle near user limits to prevent state flapping.
- **Stabilized Global Sweep:** Slowed-down global sweep (15s duration) ensures hardware capacitors settle, providing a near-perfect MPP baseline without voltage offsets.
- **Hardware Safety Architecture:** Dedicated protection layer for the board:
  - **Over-Voltage:** 80V (Vin), 60V (Vout).
  - **Over-Current:** 20A (Input/Output).
  - **Under-Voltage:** 12.5V (Vin) floor to protect auxiliary power.
  - **Thermal:** 85°C internal limit.
  - **Reverse Flow:** Active backflow detection from battery to panel.

---

## 🛠 Hardware & Platform
- **MCU:** STM32F072RBT6 (Nucleo-F072RB Development Board)
- **Framework:** STM32Cube HAL
- **Build System:** PlatformIO
- **GUI Config:** STM32CubeMX (`MPPT.ioc`)
- **Headless Operation:** Fixed USB boot lockup; the board boots and regulates **autonomously** without requiring a serial connection.

---

## 🏗 Build & Development Workflow

### Requirements
- [PlatformIO Core](https://platformio.org/)
- STM32CubeMX (optional, for peripheral reconfiguration)

### Custom Build Automation
This project uses a custom script, `setup_cubemx_env_auto.py`, which bridges the gap between STM32CubeMX and PlatformIO.
- **Automatic Constant Extraction:** The script automatically reads your PWM frequency and timer settings directly from the `MPPT.ioc` file at compile-time.
- **Link Management:** Automatically handles library linking for USB and HAL drivers.

### Commands
```bash
# Build the firmware
pio run

# Upload via Black Magic Probe (standard in platformio.ini)
pio run -t upload

# Open the serial monitor (115200 baud)
pio device monitor
```

## 📈 Auto-Tuning
The firmware supports dynamic parameter tuning over serial. A Python script is provided to automate the finding of optimal MPPT parameters (aggressiveness, step sizes, and filtering) using a hybrid machine-learning approach.

```bash
# Install dependencies
pip install pyserial

# Run the tuner (Phase 1: Random Search, Phase 2: Local Fine-Tuning)
python scripts/tune_mppt.py --port /dev/ttyACM0
```

## 📐 Hardware Design
This repository includes all necessary design files for the MPPT controller hardware in the `hardware/` directory.

- **Schematics:** SVGs and PDFs available in `hardware/schematics/`.
- **PCB Design:** Gerbers, BOM, and Pick-and-Place files located in `hardware/PCB manufacturing/`.
- **3D Files:** STEP file for case/mechanical integration in `hardware/models/`.
- **Project Files:** EasyEDA source files in `hardware/EasyEDA/` and KiCad files in `hardware/KiCad/`.

---

## 🔗 Useful Links & Resources
- **Redirect printf to USB-VCP:** [Alexey Kosinov GitHub](https://github.com/alexeykosinov/Redirect-printf-to-USB-VCP-on-STM32H7-MCU)
- **How to use C++ with STM32 HAL:** [Bare Naked Embedded](https://barenakedembedded.com/how-to-use-cpp-with-stm32-hal/)
- **STM32 ADC Multiple Channels:** [Controllerstech](https://controllerstech.com/stm32-adc-multiple-channels/)
- **Send UART over USB:** [Controllerstech](https://controllerstech.com/send-and-receive-data-to-pc-without-uart-stm32-usb-com/)

---

## 📊 Telemetry & Debugging
Real-time telemetry and configuration are provided via **USB CDC (Virtual COM Port)**.

### 🌐 Web Dashboard
The easiest way to monitor and configure the controller is via the **Web Serial Dashboard**:
👉 **[Live Dashboard](https://totallynotahackertrustmeiamadolphin.github.io/MPPT/)**

*Requires a Chrome or Edge browser. Displays data in standard units (**V**, **A**, **W**) and features a robust settings synchronization mechanism.*

### Serial Protocol
The firmware emits machine-readable **JSON packets** with descriptive fault reporting:
- `Vin_mV / Vout_mV`: Input and Output Voltages.
- `Ain_mA / Aout_mA`: Input and Output Currents.
- `state`: IDLE, SWEEPING, MPPT, CV, CC, FAULT.
- `fault_reason`: Descriptive hardware error (e.g., `INPUT_UNDERVOLTAGE`).
- `duty_x100`: PWM duty cycle as a percentage with 0.01% precision.

---

## 📏 Development Conventions

- **Variable Naming:** Always include the unit suffix in variable names (e.g., `currentIn_mA`).
- **EEPROM Safety:** The board uses a **Signature Check** (`0xABCD`) to ensure safe defaults on first flash. PWM is automatically disabled during Flash writes to prevent controller crashes.
- **Integer Math:** Perform **multiplication before division** in all scaling logic to maintain precision. Avoid `float` and `double` at all costs.

---

## 📝 License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
