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
| Q1-Q4 | **BSC030N08NS5** | Power MOSFETs | C501507 |
| U1 | **STM32F072RBT6** | MCU | C46046 |
| UA1/UB1 | **IRS21867STRPBF** | Half-Bridge Gate Driver | C52290 |
| U2/U3 | **CC6937S8-3FB020** | Isolated Current Sensor | C41421919 |
| U4 | **SCT2A25STER** | 10V Primary Aux Buck | C5124114 |
| U5 | **SY8120B1ABC** | 3.3V Logic Buck | C88474 |
| L4 | **FC-SE2822-150M** | Buck-Boost Stage Inductor | C46553544 |
| L2 | **APS0650M470A** | Aux Supply Inductor | C47327224 |
| L3 | **FTC201610S4R7MBCA** | Aux Supply Inductor | C5832346 |
| C15-C18,C20-C23 | **LKML2502A331MF** | 330uF Bulk Electrolytic Cap | C443153 |
| C_IN1/C_IN2 | **TCC1210X7R225K101MT** | Input Filter Cap | C5449052 |
| D_TVS1 | **5.0SMDJ85CA** | 80V Bus Protection | C42394457 |
| D_TVS2 | **H12VH22U** | 10V Gate protection | C20615799 |
| D_ESD | **H3V3L06B** | 3.3V Logic protection | C20615778 |
| D_BOOT | **US1M** | Bootstrap Diode | C412437 |
| D_OFF | **1N4148W** | Gate Turn-off Diode | C2001 |
| GPIO1/SPI-I2C1 | **2541WV-08P** | 8-pin Header (GPIO/SPI/I2C) | C5383116 |
| ST_LINK1/UART1 | **B4B-XH-A** | 4-pin JST-XH (ST-Link/UART) | C144395 |
| NTC1 | **PZ254V-11-02P** | 2-pin Header (NTC) | C492401 |
| H2 | **PZ254V-11-04P** | 4-pin Header | C2691448 |
| USB1 | **TYPE-C16PIN2MD** | USB-C Connector | C2765186 |
| VIN1/VOUT1 | **XT60PW-M** | Power Connector (In/Out) | C98732 |

> **Note on L4**: the part-name convention (`FC-SE2822-150M` → 15uH) implies a lower
> inductance than the previous generic placeholder spec here (33uH-47uH, Isat > 25A).
> L4 is the inductor actually placed on the Buck-Boost sheet, but the 15uH-vs-33-47uH
> gap hasn't been reconciled against `CALCULATIONS.typ` — verify before relying on this
> for the main-inductor spec.

## 3. Tooling
- **KiCad CLI**: `C:\Program Files\KiCad\10.0\bin\kicad-cli.exe`
- **Vendored Library**: `hardware\KiCad\libraries\easyeda2kicad\` (16 parts not available via PCM — symbols/footprints/3D models fetched with the `easyeda2kicad` tool, registered in `sym-lib-table`/`fp-lib-table`).
- **JLCPCB Passives**: install the `CDFER/JLCPCB-Kicad-Library` package per-machine via KiCad's Plugin & Content Manager — not vendored into the repo.

## 4. Working with Claude

Claude reads/edits the KiCad project as plain text (`.kicad_sch`/`.kicad_pcb` are S-expression
files) and runs `kicad-cli` for DRC/ERC — it cannot open KiCad's GUI or judge layout/routing
visually without a screenshot. Workflow:

- **Visual review**: paste a screenshot of a schematic sheet, PCB layout, or 3D view. Claude
  cross-checks what's visible against this Hardware Universe table and `CALCULATIONS.typ`, and
  reads the underlying `.kicad_sch`/`.kicad_pcb` for anything a screenshot can't show precisely
  (exact net names, trace widths, footprint bindings).
- **Anti-staleness check**: before a hardware PR, or on request, Claude greps every reference
  designator in the Hardware Universe table against the actual KiCad files and reports any drift
  in footprint/value/net, rather than trusting the table from memory. Keep this table limited to
  what's genuinely hard to derive from the files (rationale, "why this part") — the KiCad project
  itself is the source of truth for raw pin/footprint data.
- **Part selection & calculations**: for new components, Claude checks datasheet specs against
  the operating envelope (MOSFET Vds/Rds(on)/Qg vs. 100kHz switching losses, inductor Isat/DCR vs.
  the 20A continuous rating, TVS clamping voltage vs. the 80V rail margin, sensor bandwidth vs. the
  ~1.5kHz control loop), checks LCSC/JLCPCB stock/package availability, and documents the
  derivation in `CALCULATIONS.typ` — not just the final part number.
- **DRC/ERC enforcement**: Claude runs `kicad-cli` DRC/ERC on request or before a hardware PR and
  reports violations, so the "no merge without a clean report" mandate above is actually checked.
- **Change mechanics**: hardware edits (this file, `CALCULATIONS.typ`, KiCad files) go through a
  `hardware/<name>` branch and PR, per `CLAUDE.md` — never committed straight to `main`. Example:
  `hardware/fix-resistor-values`.
