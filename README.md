# MPPT Controller - STM32F072 Firmware

A professional-grade firmware for a Maximum Power Point Tracking (MPPT) solar charge controller, optimized for the **ARM Cortex-M0** architecture. This project implements high-frequency power conversion with advanced sensing and regulation logic.

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

### 4. Advanced Control Logic
- **Dual-Loop PID:** Integrated regulation for Constant Voltage (CV) and Constant Current (CC) charging modes.
- **MPPT Algorithms:** 
  - **Perturb & Observe (P&O):** Fast, real-time tracking of the maximum power point.
  - **Global Sweep:** Periodically scans the entire power curve to find the true peak in partial shading conditions.
- **Safety Engine:** Built-in protection for Over-Voltage (OV), Over-Current (OC), and Under-Voltage (UV).

---

## 🛠 Hardware & Platform
- **MCU:** STM32F072RBT6 (Nucleo-F072RB Development Board)
- **Framework:** STM32Cube HAL
- **Build System:** PlatformIO
- **GUI Config:** STM32CubeMX (`MPPT.ioc`)

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

# Upload via DFU
pio run -t upload

# Open the serial monitor (115200 baud)
pio device monitor
```

## 📐 Hardware Design
This repository includes all necessary design files for the MPPT controller hardware in the `hardware/` directory.

- **Schematics:** SVGs and PDFs available in `hardware/` and `hardware/scematic v1.0.pdf`.
- **PCB Design:** Gerbers, BOM, and Pick-and-Place files located in `hardware/PCB manufacturing/`.
- **3D Files:** STEP file for case/mechanical integration in `hardware/3D_PCB_STM32 MPPT_v1.1.step`.
- **Project Files:** EasyEDA source files in `hardware/EasyEDA/`.

---

## 📊 Telemetry & Debugging
Real-time telemetry and configuration are provided via **USB CDC (Virtual COM Port)**.

### 🌐 Web Dashboard
The easiest way to monitor and configure the controller is via the **Web Serial Dashboard**:
👉 **[Live Dashboard](https://totallynotahackertrustmeiamadolphin.github.io/MPPT/)**

*Requires a Chrome or Edge browser. Allows for real-time telemetry visualization, limit configuration, and sensor calibration.*

### Serial Protocol
The firmware emits machine-readable **JSON packets** at 115200 baud:
- `Vin_mV / Vout_mV`: Input and Output Voltages.
- `Ain_mA / Aout_mA`: Input and Output Currents.
- `Win_mW / Wout_mW`: Calculated Input/Output Power.
- `eff`: System efficiency percentage.
- `temp_C`: MCU internal temperature.
- `duty`: Current PWM tick values.

---

## 📏 Development Conventions

- **Variable Naming:** Always include the unit suffix in variable names (e.g., `currentIn_mA`).
- **CubeMX Safety:** Always place manual code within `/* USER CODE BEGIN */` and `/* USER CODE END */` blocks to ensure it persists after regenerating the `.ioc` file.
- **Integer Math:** Perform **multiplication before division** in all scaling logic to maintain precision. Avoid `float` and `double` at all costs.

---

## 📝 License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
