# openMPPT Hardware Documentation

## v1.1 Build Status: SUCCESS (Verified 2026-05-29)
The v1.1 PCB has been fully assembled and verified. However, several physical patches are required to achieve full functionality. These must be implemented in the next KiCad revision.

## 🛠 Critical Design Fixes (To-Do)
The following issues were identified in V1.0/V1.1 and must be implemented in the KiCad source files:

- **R1, R3 should be 5,1k but they are 5,1 ohm**, which resultet in the inability to read voltage.
- **C10, C11 where supposed to be low pass fitler** for the current signal, but they just make it unreadable.
- **the 12V buck converter is dying** (XL7005A stability issues).
- **XT60 PCB versions are switched polarity** based on wheter they are male or female.
- **the Reset button needs a pinout** fix.
- **the SWDIO/SWCLK pull downs are wrong**.
- **ADC Safety**: Add protection for PA1 (VsenseIn). It gets fried if the low-side divider resistor (R2/R4) is missing or has a cold joint.

## ⚡ High-Efficiency Optimization (Verified)
The following manual modifications have been proven to increase efficiency from 94% to **96%**:

### 1. Gate Drive Strength
- **Change**: Parallel a 10 Ω resistor on **R11, R12, R37, R38** to achieve **5 Ω** gate resistance.
- **Result**: Drastically faster switching transitions.

### 2. Dead-Time Reduction
- **Firmware Change**: Reduce `DeadTime` in `tim.c` from 48 to **16 ticks (333 ns)**.
- **Result**: Minimal body-diode conduction heat.

## PCB Design Files
- **KiCad Source**: `hardware/KiCad/`
- **Production Files**: `hardware/PCB manufacturing/`
- **3D Models**: `hardware/models/`
