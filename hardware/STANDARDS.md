# openMPPT Hardware Engineering Standards

## Project Overview
This directory contains the KiCad electronic design files for the openMPPT controller.

## Core Mandates
- **Safety First**: High-power traces (VIN, VOUT, GND, VS_A, VS_B) must be sized for 20A continuous load. Standard width is **3.5mm** (assumes 2oz copper or dual-side reinforcement).
- **Component Selection**: Prioritize the **PCM-JLCPCB** library for preferred parts whenever possible.
- **Persistent Memory (Hardware Universe)**: Every new component mapping or pinout MUST be persisted into the **Hardware Universe** section immediately.
- **Mandatory Audits**: No changes shall be merged into `main` without a clean DRC/ERC report.

# Hardware Universe (Source of Truth)

## 1. MCU Pin Mapping (STM32F072RBT6)
| MCU Pin | Signal | Function | Resistors |
| :--- | :--- | :--- | :--- |
| PA13 | SWDIO | Debug Data | 10k Pull-up, 47Ω Series |
| PA14 | SWCLK | Debug Clock | 10k Pull-down, 47Ω Series |
| BOOT0 | BOOT0 | Boot Mode | 10k Pull-down |
| PC10 | LED | Heartbeat | Active-Low |

## 2. Power Stage Components (v1.3 Finalized)
| Ref | Part Number | Function | LCSC # |
| :--- | :--- | :--- | :--- |
| Q5-Q8 | **BRCS030N10SHRA** | Power MOSFETs | C46962478 |
| U8 | **SCT2A25STER** | 10V Primary Aux Buck | C5124114 |
| U9 | **SY8120B1ABC** | 3.3V Logic Buck | C88474 |
| U4/U5 | **CC6937S8-3FB020** | Isolated Current Sensor | C5295991 |
| D_TVS1 | **5.0SMDJ85CA** | 80V Bus Protection | C42394457 |
| D_TVS2 | **H12VH22U** | 10V Gate protection | C20615799 |
| D_ESD | **H3V3L06B** | 3.3V Logic protection | C20615778 |
| D_BOOT | **US1M** | Bootstrap Diode | C412437 |
| D_OFF | **1N4148W** | Gate Turn-off Diode | C2001 |
| L_MAIN | **33uH-47uH** | Main Inductor | **Isat > 25A REQ** |

## 3. Tooling
- **KiCad CLI**: `C:\Program Files\KiCad\10.0\bin\kicad-cli.exe`
- **Library Path**: `hardware\KiCad\libraries\jlcpcb\`
