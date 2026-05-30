# openMPPT Hardware v1.3 Roadmap

This document outlines the development phases for the openMPPT v1.3 hardware revision. The goal is to evolve the design from the v1.1 baseline into a high-performance, 80V/20A capable, and cost-optimized controller.

## Phase 1: Critical Stability & Bug Fixes
*Goal: Resolve all known hardware issues identified in the v1.1 bench test.*

- [ ] **Voltage Sensing Correction**: Change `R1`, `R3` from 5.1 Ω to **5.1 kΩ** to enable correct voltage readings.
- [ ] **Connector Standardization**: Use Male `XT60PW-M` on both `VIN` and `VOUT` to resolve polarity mismatch and simplify BOM.
- [x] **Robust 10V Gate Drive**: Replace the unstable **XL7005A** with a robust 100V-rated buck converter (SCT2A25) tuned to **10V** instead of 12V to optimize gate driver thermals. [CALCULATIONS COMPLETE, SCHEMATIC PENDING]
- [x] **Cascaded 3.3V Logic Supply**: Implement a tiny synchronous buck (SY8120) to step down the 10V rail to 3.3V, isolating the MCU from 80V load dumps. [CALCULATIONS COMPLETE, SCHEMATIC PENDING]
- [ ] **Gate Drive Optimization**: Permanently change gate resistors (`R11`, `R12`, `R37`, `R38`) to **5.1 Ω** to match proven 96% efficiency tuning.
- [ ] **Reset & Debug Pinout**: 
    - Fix the Reset button pinout.
    - Correct the `SWDIO`/`SWCLK` pull-down configuration.
- [ ] **Filter Cleanup**: Remove or recalculate `C10`, `C11` to prevent current signal distortion.

## Phase 2: UI Expansion & Peripheral Headers
*Goal: Formalize the user interface and expansion capabilities.*

- [ ] **Dedicated Nokia 5110 Header**: Create a standardized 8-pin header for the LCD display (SPI1 + DC/CE/RST + Power).
- [ ] **UI Expansion Header**: Add a header for future input devices, including:
    - 2x Analog inputs (for potentiometers).
    - 3x Digital inputs (for a rotary encoder with push-button).
- [ ] **SWO Support**: Break out the SWO pin to the debug header for better real-time profiling.

## Phase 3: Power Scaling & Sensor Upgrades (80V/20A)
*Goal: Scale the analog front-end and power stage for higher limits.*

- [ ] **Inductor Sizing**: Replace 100µH inductors with optimized **33µH - 47µH** components to reduce DCR and improve transient response while maintaining CCM.
- [ ] **Low-ESR Bulk Capacitance**: Implement a parallel bank of capacitors (e.g., 4x 100µF-220µF) to hit the **<25mΩ ESR** target required for 100mV ripple at 20A. Add ceramic MLCCs at switching nodes.
- [ ] **80V Voltage Dividers**: 
    - Recalculate and update `R_top` values to support sensing up to **82V-85V** (current limit ~69V).
    - Update ADC scaling constants in `hardware/CALCULATIONS.md`.
- [ ] **Hall Effect Current Sensing**:
    - Replace shunt-based sensing (`INA240`) with **ACS712 (or similar)** Hall Effect sensors for improved isolation and reduced voltage drop.
- [ ] **20A Thermal Design**: 
    - Widen high-power traces and implement large polygon pours.
    - Add thermal vias to MOSFET pads.
- [ ] **Overvoltage Protection**: Integrate **TVS diodes** on `VIN` and `VOUT`. Add a 15V "Crowbar" TVS on the 12V rail to protect the MCU if the primary regulator fails short. Ensure gate drivers have strong 10kΩ pull-downs for intentional safe brownout when VIN is lost.

## Phase 4: Architectural Optimization & Cost Reduction
*Goal: Improve manufacturability, size, and component cost.*

- [ ] **SMD Migration**: Transition from TO-220 MOSFETs to **SMD (PowerPDFN or DPAK)**.
- [ ] **Bootstrap Gate Driving**: Evaluate replacing isolated `B1212S` converters with a **bootstrap circuit**.
- [ ] **Unified Dev Header**: Combine Reset, UART (TX/RX), and SWD into a single 10-pin shrouded header.

## Phase 5: Verification & Manufacturing
- [ ] **Full DRC/ERC Audit**: Zero errors in KiCad using `flatpak run --command=kicad-cli org.kicad.KiCad`.
- [ ] **BOM Scrubbing**: Ensure all components prioritize the `PCM-JLCPCB` library.
- [ ] **Generation of Production Files**: Gerbers, Drill, BOM, and CPL.
