# openMPPT Hardware v1.3 Roadmap

This document outlines the development phases for the openMPPT v1.3 hardware revision. The goal is to evolve the design from the v1.1 baseline into a high-performance, 80V/20A capable, and cost-optimized controller.

## Phase 1: Critical Stability & Bug Fixes
*Goal: Resolve all known hardware issues identified in the v1.1 bench test.*

- [x] **Voltage Sensing Correction**: Change `R1`, `R3` to **200k/4.7k** to enable correct voltage readings up to 80V. [DONE]
- [x] **Connector Standardization**: Use Male `XT60PW-M` on both `VIN` and `VOUT`. [DONE]
- [x] **Robust 10V Gate Drive**: Replace XL7005A with **SCT2A25** tuned to **10V**. [DONE]
- [x] **Cascaded 3.3V Logic Supply**: Implement **SY8120** stepping down from 10V to 3.3V. [DONE]
- [x] **Gate Drive Optimization**: Permanently change gate resistors to **5.1 Ω** with **1N4148W** turn-off diodes. [DONE]
- [ ] **Reset & Debug Pinout**: 
    - Fix the Reset button pinout.
    - Implement **10k Pull-up (SWDIO)**, **10k Pull-down (SWCLK)**, and **47Ω series resistors**.
- [x] **Filter Cleanup**: Recalculated VSense LPF to **3.47 kHz** (10nF). [DONE]

## Phase 2: UI Expansion & Peripheral Headers
*Goal: Formalize the user interface and expansion capabilities.*

- [ ] **Dedicated Nokia 5110 Header**: Standardized 8-pin header (SPI1 + DC/CE/RST + Power).
- [ ] **UI Expansion Header**: 2x Analog (Pots), 3x Digital (Encoder).
- [ ] **SWO Support**: Break out SWO to debug header.

## Phase 3: Power Scaling & Sensor Upgrades (80V/20A)
*Goal: Scale the analog front-end and power stage for higher limits.*

- [x] **Inductor Sizing**: Targeted **33µH - 47µH** with **Isat > 25A**. [SPECS DEFINED]
- [x] **Low-ESR Bulk Capacitance**: Parallel bank of 4x **330µF 100V** (Ymin LKML). [DONE]
- [x] **Hall Effect Current Sensing**: Replaced INA240 with isolated **CC6937**. [DONE]
- [ ] **20A Thermal Design**: 
    - Widen high-power traces.
    - Implement massive thermal via grid for SMD MOSFETs (if used).
- [x] **Overvoltage Protection**: Integrated **5.0SMDJ85CA** (80V) and **H12VH22U** (10V). The
      planned 3.3V TVS (**H3V3L06B**, D9) was removed after datasheet verification found it
      couldn't clear the STM32's 4.0V absolute max at any current beyond a trivial ESD event —
      see `CALCULATIONS.typ`. That rail's protection now rests on the SY8120B1's own regulation
      plus the upstream Vin OV/UV limits. [DONE]

## Phase 4: Architectural Optimization & Cost Reduction
- [x] **SMD Migration**: Transition to **BSC030N08NS5** or keeping **BRCS030N10SHRA** (TO-220) based on board space.
- [ ] **Unified Dev Header**: Combined Reset, UART, and SWD.
- [x] **Switching Frequency: 200kHz**: `TIMER_PERIOD` (`system_config.h`/`MPPT.ioc`) has
      actually computed to 200kHz since the earliest commit in this repo's history — this
      was previously undocumented, not a pending change. Component re-check done — see
      `CALCULATIONS.typ` Section 8: MOSFET switching loss roughly doubles vs. a
      hypothetical 100kHz (thermal design from this phase must land first/alongside), gate
      driver has ample margin, main inductor target halves to ~25uH. What genuinely *was*
      missing until the bootstrap-refresh firmware update: `DITHER_TABLE_SIZE` was never
      paired with the halved period (8→16 fix), and the PWM hardware map needed a
      bootstrap-cap-safe rework for v1.3's bootstrap-cap gate drive (no isolated supply).

## Phase 5: Verification & Manufacturing
- [ ] **Full DRC/ERC Audit**: Zero errors in KiCad using `kicad-cli`.
- [ ] **BOM Scrubbing**: Final LCSC part verification.
- [ ] **Generation of Production Files**: Gerbers, Drill, BOM, and CPL.
