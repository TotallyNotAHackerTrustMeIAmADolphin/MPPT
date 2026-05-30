# openMPPT Hardware Engineering Standards

## Project Overview
This directory contains the KiCad electronic design files for the openMPPT controller.

## Core Mandates
- **Safety First**: High-power traces (VIN, VOUT, GND, VS_A, VS_B) must be sized for 20A continuous load. Standard width is **3.5mm** (assumes 2oz copper or dual-side reinforcement).
- **Component Selection**: Prioritize the **PCM-JLCPCB** library for preferred parts whenever possible to ensure compatibility with JLCPCB's assembly service.
- **Datasheet Handling**: Always check the local `hardware/Datasheets` directory first. If local PDF extraction fails, use `web_fetch` to read the PDF directly from the project's GitHub repository URL rather than relying on external Google searches. Only use external searches if the datasheet is not present in the repository.
- **Persistent Memory (Hardware Universe)**: To optimize context usage, every time a new component mapping, pinout, or critical hardware fact is discovered by 'digging' through files, it MUST be persisted into the **Hardware Universe** section of this file immediately. This serves as the 'source of truth' for the project's physical implementation.
- **Mandatory Audits**: No changes shall be merged into `main` without a clean DRC/ERC report (zero errors, justified warnings).
- **Tooling**: Use the Flatpak-based `kicad-cli` for all automated audits and exports.

## Primary Workflows

### 1. AI-Human Collaboration Workflow (The "Hybrid Drop")
This workflow optimizes for both AI capabilities and human spatial reasoning during schematic updates.
- **Phase 1 (Calculation)**: The AI fetches datasheets, performs sizing calculations (e.g., feedback dividers, inductor sizing), and documents the math in `CALCULATIONS.md`.
- **Phase 2 (Implementation)**: The AI provides a specific checklist of components to add. The Human Engineer places these symbols manually in the KiCad GUI and routes the wires.
- **Phase 3 (Property Assignment)**: Once the symbols are placed, the AI scans the schematic S-expressions to bulk-update symbol properties (Values, Footprints, Manufacturer Part Numbers, Links).
- **Phase 4 (Audit)**: The AI executes ERC/DRC audits via `kicad-cli` to verify connectivity and rules.

### 2. Running Audits (Flatpak Environment)
To run audits, use the following commands:

**Electrical Rules Check (ERC):**
```bash
flatpak run --command=kicad-cli org.kicad.KiCad sch erc --exit-code-violations hardware/KiCad/openMPPT_v1.1.kicad_sch
```

**Design Rules Check (DRC):**
```bash
flatpak run --command=kicad-cli org.kicad.KiCad pcb drc --exit-code-violations hardware/KiCad/openMPPT_v1.1.kicad_pcb
```

### 2. Manufacturing Exports
Always generate a fresh set of manufacturing files before tagging a release.

**Gerbers:**
```bash
flatpak run --command=kicad-cli org.kicad.KiCad pcb export gerber --output hardware/manufacturing/gerbers/ hardware/KiCad/openMPPT_v1.1.kicad_pcb
```

**BOM Export:**
```bash
flatpak run --command=kicad-cli org.kicad.KiCad sch export bom --output hardware/manufacturing/BOM.csv hardware/KiCad/openMPPT_v1.1.kicad_sch
```

## Stackup & Manufacturing Specs
- **Layers**: 2 Layers (minimum 2oz copper recommended for 20A).
- **Material**: FR-4 (TG130-140).
- **Surface Finish**: HASL Lead-Free or ENIG.
- **Trace/Space**: 0.2mm / 0.2mm minimum for logic; 0.5mm minimum clearance for high-voltage nets (>60V).

## Revision History
- **v1.1**: Current stable baseline (INA240 current sensing).
- v1.3: (In Progress) Transitioning to ACS712 sensors and thermal optimizations.

# Hardware Universe (Source of Truth)

## 1. MCU Pin Mapping (STM32F072RBT6)
| MCU Pin | Signal | Status | Discovery Source |
| :--- | :--- | :--- | :--- |
| Pin 8 | PC0 | Unused / Available for UI | ERC Report v1.1 |
| Pin 9 | PC1 | Unused / Available for UI | ERC Report v1.1 |
| Pin 10 | PC2 | Unused / Available for UI | ERC Report v1.1 |
| Pin 11 | PC3 | Unused / Available for UI | ERC Report v1.1 |
| Pin 14 | PA0 | Unused / Available for UI | ERC Report v1.1 |
| Pin 22 | PA6 | Unused / Available for UI | ERC Report v1.1 |
| Pin 27 | PB1 | Unused / Available for UI | ERC Report v1.1 |
| Pin 28 | PB2 | Unused / Available for UI | ERC Report v1.1 |
| Pin 50 | PA15 | Unused / Available for UI | ERC Report v1.1 |
| Pin 52 | PC11 | Unused / Available for UI | ERC Report v1.1 |
| Pin 53 | PC12 | Unused / Available for UI | ERC Report v1.1 |
| Pin 58 | PB6 | Unused / Available for UI | ERC Report v1.1 |
| Pin 59 | PB7 | Unused / Available for UI | ERC Report v1.1 |
| Pin 61 | PB8 | Unused / Available for UI | ERC Report v1.1 |
| Pin 62 | PB9 | Unused / Available for UI | ERC Report v1.1 |
| PB12 | RST | Nokia 5110 RST (SPI1) | v1.3 Plan |
| PB13 | DC | Nokia 5110 DC (SPI1) | v1.3 Plan |
| PB14 | CE | Nokia 5110 CE (SPI1) | v1.3 Plan |
| PB15 | LED | Nokia 5110 Backlight | v1.3 Plan |
| PB3 | SCK | SPI1 CLK (Nokia 5110) | v1.3 Plan |
| PB5 | MOSI | SPI1 DIN (Nokia 5110) | v1.3 Plan |
| PA0 | POT1 | Analog Potentiometer 1 | v1.3 Plan |
| PC0 | POT2 | Analog Potentiometer 2 | v1.3 Plan |
| PC1 | ENC_A | Rotary Encoder Phase A | v1.3 Plan |
| PC2 | ENC_B | Rotary Encoder Phase B | v1.3 Plan |
| PC3 | ENC_SW | Rotary Encoder Push-Button | v1.3 Plan |
| PC10 | LED | Active-Low Heartbeat | GEMINI.md Main |

## 2. Power Stage Components (v1.1 Baseline)
| Ref | Part Number | Function | Notes |
| :--- | :--- | :--- | :--- |
| Q5-Q8 | CSD19505KCS | Buck-Boost MOSFETs | TO-220-3 Package |
| UA, UB | IRS21867STRPBF | Half-Bridge Drivers | Driven by TIM1 |
| U4, U5 | INA240A4DR | Current Sense Amp | Gain: 200 V/V |
| U8 | **SCT2A25** (v1.3) | 10V Primary Aux Buck | **Robust 100V Replacement**. JLCPCB Extended (Preferred) |
| U9 | **SY8120** (v1.3) | 3.3V Logic Sync Buck | **Cascaded from 10V**. JLCPCB Extended |
| R11, R12, R37, R38 | 5.1 Ω (v1.3) | Gate Resistors | Upgraded from 10 Ω for 96% Eff. |
| R1, R3 | 5.1 kΩ (v1.3) | Voltage Sensing Div | Fixes 5.1 Ω error in v1.1 |
| VIN | XT60PW-M | Input Connector | Pin 1: GND, Pin 2: VIN (+) |
| VOUT | XT60PW-M (v1.3) | Output Connector | Standardized to Male (Pin 1: GND, Pin 2: VOUT+) |

