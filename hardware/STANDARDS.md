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
| L4 | **HCZH-3218-150-M** | Buck-Boost Stage Inductor | C53699952 |
| L2 | **APS0650M470A** | Aux Supply Inductor | C47327224 |
| L3 | **FTC201610S4R7MBCA** | Aux Supply Inductor | C5832346 |
| C15-C18,C20-C23 | **LKML2502A331MF** | 330uF Bulk Electrolytic Cap | C443153 |
| C_IN1/C_IN2 | **TCC1210X7R225K101MT** | Input Filter Cap | C5449052 |
| D10/D11 | **5.0SMDJ85CA** | 80V Bus Protection | C42394457 |
| D8 | **H12VH22U** | 10V Gate protection | C20615799 |
| D9 | **H3V3L06B** | 3.3V Logic protection | C20615778 |
| D1/D4 | **SS210** | Gate Driver Bootstrap Diode | C14996 |
| D2/D3/D5/D6 | **SS210** | Gate Discharge Diode | C14996 |
| D7 | **SS510** | Aux Buck (SCT2A25) Catch Diode | C7420368 |
| GPIO1/SPI-I2C1 | **2541WV-08P** | 8-pin Header (GPIO/SPI/I2C) | C5383116 |
| ST_LINK1/UART1 | **B4B-XH-A** | 4-pin JST-XH (ST-Link/UART) | C144395 |
| NTC1 | **PZ254V-11-02P** | 2-pin Header (NTC) | C492401 |
| H2 | **PZ254V-11-04P** | 4-pin Header | C2691448 |
| USB1 | **TYPE-C16PIN2MD** | USB-C Connector | C2765186 |
| VIN1/VOUT1 | **XT60PW-M** | Power Connector (In/Out) | C98732 |

> **L4 changed from `FC-SE2822-150M` to `HCZH-3218-150-M`** (YOUCHI, LCSC `C53699952`,
> $3.86, 88 in stock) — applied to `buck_boost.kicad_sch`, *not yet applied to the PCB*
> (its footprint uses a different pad shape/format than what's currently placed, so it
> needs a deliberate footprint swap in KiCad, not a drop-in text edit).
>
> Same 15uH as the original part, so the ripple picture is unchanged: clears the
> <100mV target at 200kHz (78mV) but not at the current 100kHz firmware setting (157mV)
> — a real, accepted gap since L4 was deliberately sized on the assumption that the
> 200kHz switch happens. What changed is DCR (3.5mOhm -> 2.2mOhm, cutting conduction
> loss from 1.40W to 0.88W) for a $1.56 premium over the original part.
>
> This was chosen over several alternatives considered along the way (see
> `CALCULATIONS.typ` Section 8 for the full comparison): the absolute cheapest option
> was the original part itself ($2.30); a same-inductance/near-free upgrade
> (`C54830921`, +$0.57, 89% Isat margin) was available; and a higher-margin option
> (`RSEQ32-220M`/`C37634008`, $6.26, 22uH, 304% Isat margin, 0.36W conduction loss) was
> evaluated and rejected as solving for more margin than the design needs, at a stock
> and price cost that didn't justify it for a prototype build.

> **Why SS210 for both D1/D4 (bootstrap) and D2/D3/D5/D6 (gate discharge)**: D2/D3/D5/D6
> only ever see the ~10V gate-drive swing, so SS210's 100V/2A rating is more margin than
> that role strictly needs — a smaller Schottky (e.g. `B5819W`, 40V/1A, same `D_SOD-123`
> footprint already placed at those 4 spots) would be a tighter fit. Deliberately not
> doing that: one diode part number across all six positions beats a marginally-better
> fit on four of them — fewer reels at assembly, less inventory, less chance of a mix-up
> during hand work. SS210 comfortably meets every requirement of both roles (voltage,
> surge current, Schottky switching speed), just with headroom to spare on the discharge
> role.

> **D2/D3/D5/D6 footprint fixed, placement still pending**: these previously had
> `D_SOD-123` footprints on the PCB despite the schematic (correctly) specifying
> `D_SMA` for all six SS210s — now corrected to `D_SMA`, with pad-net mapping/polarity
> preserved. Since the board isn't routed yet, this surfaced the expected next step:
> `D_SOD-123` (~2.7x1.6mm) is much smaller than `D_SMA` (~5x2.6mm), so D2/D3/D5/D6
> need to be spread apart from each other and from nearby parts (R9, R10, R17, R18,
> R36-R39, UA1, UB1) during placement — normal pre-routing layout work, not a bug.

## 3. Tooling
- **KiCad CLI**: `C:\Program Files\KiCad\10.0\bin\kicad-cli.exe`
- **Vendored Library**: `hardware\KiCad\libraries\easyeda2kicad\` (16 parts not available via PCM — symbols/footprints/3D models fetched with the `easyeda2kicad` tool, registered in `sym-lib-table`/`fp-lib-table`).
- **JLCPCB Passives**: install the `CDFER/JLCPCB-Kicad-Library` package per-machine via KiCad's Plugin & Content Manager — not vendored into the repo.

## 4. Working with Claude

Claude reads/edits the KiCad project as plain text (`.kicad_sch`/`.kicad_pcb` are S-expression
files) — it cannot open KiCad's GUI or judge layout/routing visually without a screenshot.
Workflow:

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
- **DRC/ERC enforcement**: this is a people job. Claude does not run `kicad-cli` DRC/ERC, proactively
  or on request — board correctness review stays with the user. The "no merge without a clean
  report" mandate above is checked by the user, not by Claude.
- **Change mechanics**: hardware edits (this file, `CALCULATIONS.typ`, KiCad files) go through a
  `hardware/<name>` branch and PR, per `CLAUDE.md` — never committed straight to `main`. Example:
  `hardware/fix-resistor-values`.
