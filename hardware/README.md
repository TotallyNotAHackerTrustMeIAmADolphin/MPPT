# openMPPT Hardware Documentation

## ⚙️ Setup Requirements (per machine)
Before opening `hardware/KiCad/openMPPT_v1.3.kicad_pro`:
- **KiCad 10.0** — matches this project's file format (`generator_version "10.0"`); an older
  major version will prompt to upgrade the files.
- **Install the `JLCPCB-Kicad-Library` package** (author: CDFER) via KiCad's
  **Tools → Plugin and Content Manager**. This is the *official* PCM package — don't manually
  download/vendor a copy of it yourself, that's what causes the `KICAD8_3RD_PARTY` environment
  variable to stay unset and 3D models (switches, TVS diodes, generic passives - anything from
  the `PCM_JLCPCB` footprint library) to silently fail to render. Restart KiCad after installing
  so the variable takes effect.
- Everything else (the `easyeda2kicad`-sourced symbols/footprints/3D models for parts not on
  LCSC's basic-parts list) is already vendored in `hardware/KiCad/libraries/` and needs no setup.

## v1.3 Component Selection Summary
| Component | Part Number | Function | Rationale |
| :--- | :--- | :--- | :--- |
| Primary Buck IC | SCT2A25STER | 10V Aux Supply | High efficiency, 100V rating for 80V VIN |
| Logic Buck IC | SY8120B1ABC | 3.3V Logic Supply | Cascaded architecture for MCU safety |
| Power MOSFET | BSC030N08NS5 | Buck-Boost Stage | SMD (PG-TDSON-8-EP), 80V/100A, $R_{DS(on)}$ 3.0mΩ, $Q_g$ 76nC max |
| Bootstrap Diode | US1M | Gate Driver Supply | 1kV rating for transient protection |
| Gate-Off Diode | 1N4148W | MOSFET Turn-off | 4ns high-speed switching for low loss |
| Current Sensor | CC6937S8-3FB020 | Hall Effect Sensing | 3.3V compatible, isolated, 20A range |
| 10V TVS Diode | H12VH22U | Gate Drive Protection | 12V standoff, 6kW surge handled |
| 80V TVS Diode | 5.0SMDJ85CA | Main Rail Protection | 85V standoff, industrial 5kW capacity |

*(See `CALCULATIONS.typ` for full design justification and E24/E96 resistor value derivations.)*

## v1.1 Build Status: SUCCESS (Verified 2026-05-29)
The v1.1 PCB has been fully assembled and verified, v1.1 means the v1.0 but with some patches hand soldered to the board. However, several physical patches are required to achieve full functionality. These must be implemented in the next KiCad revision.

## 🛠 Critical Design Fixes (Implemented in v1.3)
- **Voltage Sensing**: Updated to **200k/4.7k** for 80V range.
- **Current Filter**: C10, C11 removed; moved to isolated **CC6937**.
- **Aux Power**: Replaced XL7005A with **SCT2A25/SY8120** cascaded setup.
- **Connectors**: Standardized on **Male XT60PW-M**.
- **Gate Drive**: Standardized on **5.1Ω** resistors with high-speed **1N4148W** bypass.
- **Protection**: TVS protection on the 80V main rail and 10V gate-drive rail. The 3.3V logic
  rail's originally-planned TVS (H3V3L06B, D9) was removed after datasheet verification showed
  it's an ESD-class part whose own clamping voltage exceeds the STM32's 4.0V absolute max at any
  current beyond a trivial ESD event — no purpose-built power-rail TVS clears 4.0V either (a
  physics limit of 3.3V-class parts). That rail's fault protection now rests on the SY8120B1's
  own regulation and the upstream Vin OV/UV limits already enforced in firmware.

## PCB Design Files
- **KiCad Source**: `hardware/KiCad/`
- **Production Files**: `hardware/PCB manufacturing/`
- **Vendored 3D Models/Symbols/Footprints**: `hardware/KiCad/libraries/easyeda2kicad/` (see
  "Setup Requirements" above for the one library that isn't vendored)
