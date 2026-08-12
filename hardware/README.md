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
| 10V TVS Diode | H12VH22U | Gate Drive Protection | 12V standoff, 6kW surge handled |
| 80V TVS Diode | 5.0SMDJ85CA | Main Rail Protection | 85V standoff, industrial 5kW capacity |
| 3.3V ESD Diode | H3V3L06B | Logic Protection | 3.3V standoff, DFN0603 for compact assembly |

*(See `CALCULATIONS.md` for full design justification and E24/E96 resistor value derivations.)*

## v1.1 Build Status: SUCCESS (Verified 2026-05-29)
The v1.1 PCB has been fully assembled and verified, v1.1 means the v1.0 but with some patches hand soldered to the board. However, several physical patches are required to achieve full functionality. These must be implemented in the next KiCad revision.

## 🛠 Critical Design Fixes (Implemented in v1.3)
- **Voltage Sensing**: Updated to **200k/4.7k** for 80V range.
- **Current Filter**: C10, C11 removed; moved to isolated **CC6937**.
- **Aux Power**: Replaced XL7005A with **SCT2A25/SY8120** cascaded setup.
- **Connectors**: Standardized on **Male XT60PW-M**.
- **Gate Drive**: Standardized on **5.1Ω** resistors with high-speed **1N4148W** bypass.
- **Protection**: Added comprehensive TVS/ESD protection on all rails.

## PCB Design Files
- **KiCad Source**: `hardware/KiCad/`
- **Production Files**: `hardware/PCB manufacturing/`
- **3D Models**: `hardware/models/`
