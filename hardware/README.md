# openMPPT Hardware Documentation

## v1.3 Component Selection Summary
| Component | Part Number | Function | Rationale |
| :--- | :--- | :--- | :--- |
| Primary Buck IC | SCT2A25STER | 10V Aux Supply | High efficiency, 100V rating for 80V VIN |
| Logic Buck IC | SY8120B1ABC | 3.3V Logic Supply | Cascaded architecture for MCU safety |
| Power MOSFET | BRCS030N10SHRA | Buck-Boost Stage | Drop-in (TO-220), 100V/212A, matches $Q_g$ of 76nC |
| Bootstrap Diode | US1M | Gate Driver Supply | 1kV rating for transient protection |
| Gate-Off Diode | 1N4148W | MOSFET Turn-off | 4ns high-speed switching for low loss |
| Current Sensor | CC6937S8-3FB020 | Hall Effect Sensing | 3.3V compatible, isolated, 20A range |

*(See `CALCULATIONS.md` for full design justification and E24/E96 resistor value derivations.)*

## v1.1 Build Status: SUCCESS (Verified 2026-05-29)
The v1.1 PCB has been fully assembled and verified, v1.1 means the v1.0 but with some patches hand soldered to the board. However, several physical patches are required to achieve full functionality. These must be implemented in the next KiCad revision.

## 🛠 Critical Design Fixes (To-Do)
The following issues were identified in V1.0/V1.1 and must be implemented in the KiCad source files:

- **R1, R3 should be 5,1k but they are 5,1 ohm**, which resultet in the inability to read voltage.
- **C10, C11 where supposed to be low pass fitler** for the current signal, but they just make it unreadable.
- **the 12V buck converter is dying** (XL7005A stability issues).
- **XT60 PCB versions are switched polarity** based on wheter they are male or female.
- **the Reset button needs a pinout** fix.
- **the SWDIO/SWCLK pull downs are wrong**.
- TVS diodes for overvoltage surges
- 5 ohm resistors for gate instead of 10 ohm

## nice to have todo:
- better dev header (reset, and uart all in one plug)
- SWO ?
- SMD Mosfets
- smaller Board size
	- fewer capacitors
- bootsrap capacitor instead of isolated 12v regulators
- more efficiency (faster switching?)
- hall effect current sensors
- maybe switch to another MCU ? for example the bluepill one
- maybe change the 3v3 regulator to an LDO

## Design Rules:
- use JLPCB prefered parts library as often as posible (PCM-JLCPCB)
	- if not available, use LCSC partss


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
