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
| L2 | **GXDR1207-470MT** | Aux Supply Inductor (SCT2A25/10V) | C52196367 |
| L3 | **FTC201610S4R7MBCA** | Aux Supply Inductor | C5832346 |
| C15-C18,C20-C23 | **LKML2502A331MF** | 330uF Bulk Electrolytic Cap | C443153 |
| C_IN1/C_IN2 | **TCC1210X7R225K101MT** | Input Filter Cap | C5449052 |
| D10/D11 | **5.0SMDJ85CA** | 80V Bus Protection | C42394457 |
| D8 | **H12VH22U** | 10V Gate protection | C20615799 |
| D1/D4 | **SS210** | Gate Driver Bootstrap Diode | C14996 |
| D2/D3/D5/D6 | **SS210** | Gate Discharge Diode | C14996 |
| D7 | **SS510** | Aux Buck (SCT2A25) Catch Diode | C18199171 |
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
> <100mV target at the firmware's actual 200kHz operating point (78mV) but would not
> clear it at a hypothetical 100kHz (157mV) — L4 was deliberately sized against the real
> 200kHz operating point (`TIMER_PERIOD=240` has computed to 200kHz since the earliest
> commit in this repo's history, confirmed via git log, not merely assumed). What changed
> is DCR (3.5mOhm -> 2.2mOhm, cutting conduction loss from 1.40W to 0.88W) for a $1.56
> premium over the original part.
>
> This was chosen over several alternatives considered along the way (see
> `CALCULATIONS.typ` Section 8 for the full comparison): the absolute cheapest option
> was the original part itself ($2.30); a same-inductance/near-free upgrade
> (`C54830921`, +$0.57, 89% Isat margin) was available; and a higher-margin option
> (`RSEQ32-220M`/`C37634008`, $6.26, 22uH, 304% Isat margin, 0.36W conduction loss) was
> evaluated and rejected as solving for more margin than the design needs, at a stock
> and price cost that didn't justify it for a prototype build.

> **L2 changed from `APS0650M470A` to `GXDR1207-470MT`** (FAUKU, LCSC `C52196367`,
> $0.207, 11735 in stock) — applied to both `power.kicad_sch` and the PCB. Same 47uH,
> but Isat goes from 2.62A to 4.0A, now matching `SCT2A25STER`'s own 4A peak current
> limit instead of saturating well below it, and DCR drops from 227mOhm to 90mOhm.
> Driven by a hypothetical heavier load (e.g. a fan) beyond the gate-driver/logic load
> the rail was originally sized for. Package grew from 7.7x6.6mm to 12.5x12.5mm SMD.

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

## 3. PCB Net Classes (Design Rules)

Defined in `openMPPT_v1.3.kicad_pro` → `net_settings` (Board Setup → Net Classes in KiCad).
Assignment is by netclass pattern, not manual per-net tagging, so newly added nets that match
a pattern pick up the right width automatically.

| Class | Track Width | Clearance | Pattern(s) | Nets covered |
| :--- | :--- | :--- | :--- | :--- |
| `Power_20A` | 3.5mm | 0.5mm | `VIN`, `IN_CAP`, `VOUT`, `OUT_CAP`, `VS_*` | Input/output current path traces — the actual 20A-carrying nets, confirmed at the pad level (CC6937 `IP+`/`IP-` primary pins on U2/U3 sit on `IN_CAP`/`VIN` and `OUT_CAP`/`VOUT`, not on `ISENSEIN`/`ISENSEOUT`) |
| `AuxSupply` | 0.6mm | 0.2mm (default) | `3V3`, `+10V` | Logic and gate-driver supply rails — meaningful current but nowhere near 20A |
| `Default` | 0.2mm | 0.2mm | everything else, **including `GND`** | Signal/control, plus GND — a poured zone doesn't care about netclass `track_width` (that only governs hand-drawn trace segments; zone fill width is set by the zone's own "minimum thickness"), so forcing GND to 3.5mm would only have added friction on incidental GND stub traces. Isolation is unaffected: DRC clearance between two nets uses the larger of the two netclasses' clearance, so the GND pour still gets pushed 0.5mm off `VIN`/`VOUT`/etc. via *their* clearance regardless of what class GND itself is in. |

**Deliberately excluded from `Power_20A` despite the naming**: `ISENSEIN`/`ISENSEOUT` are the
CC6937's isolated analog *output* pins (mA-level signal to the MCU ADC), and `SHUNT_IN+` is
U4's (aux 10V buck) local input tap off VIN with its own bypass caps — also low current. Traced
via pad `pinfunction` in the PCB, not assumed from the name.

**Open items, not yet resolved**:
- `Power_20A`'s 3.5mm width assumes **2oz copper** per the Core Mandate above, but the board has
  no explicit stackup block (`.kicad_pcb` has no `(stackup ...)` section) — it's silently on
  KiCad's default, likely 1oz. **Set this explicitly**: Board Setup → Physical Stackup → Copper
  Layers → 70µm (2oz) for both F.Cu/B.Cu, or the 3.5mm figure understates what's needed. Not
  done here — no `kicad-cli` available on this machine to validate a hand-edited stackup block,
  and a malformed one would be a bad way to find out.
- `Power_20A` clearance (0.5mm) is a rough IPC-2221-style external/uncoated margin for 80V, not
  derived from a documented calculation — sanity-check it before relying on it, ideally with the
  actual creepage table (altitude/coating/pollution-degree assumptions all matter and aren't
  pinned down anywhere yet).

## 4. JLCPCB Manufacturability Floor (Design Rules)

Board-wide DRC floor — applies regardless of net, on top of (not instead of) the net classes
above. Targets JLCPCB's **standard 2-layer FR4 service** (no HDI/special-process surcharge),
at this board's chosen **2oz copper, 1.6mm thickness** (both already JLCPCB's own defaults for
"2oz" and "standard thickness" respectively). Split across two places, both must agree:

- `openMPPT_v1.3.kicad_dru` (Board Setup → Custom Rules): track/spacing, hole/via/annular-ring,
  silkscreen, edge clearance.
- `openMPPT_v1.3.kicad_pro` → `board.design_settings.rules` (Board Setup → Design Rules →
  Constraints): the same floors, as the simple global minimums KiCad checks first.

| Parameter | Value | Note |
| :--- | :--- | :--- |
| Track width / spacing | 0.2mm | JLCPCB's published 2oz minimum is 0.16mm (6.5mil) — 0.2mm (8mil) is used instead as the practical margin recommended for 2oz copper (thicker copper etches less precisely than 1oz) |
| Via/PTH drill | 0.3mm min | JLCPCB's absolute technical minimum is 0.15mm, but sub-~0.3mm on the standard 2-layer service tends to trigger their special-process fee — 0.3mm stays in the no-surcharge tier |
| Via finished diameter | 0.7mm min | drill (0.3mm) + 2× annular ring (0.2mm) |
| Annular ring (via/PTH) | 0.2mm min | JLCPCB's recommended value (vs. an 0.13mm absolute minimum) — again biased toward 2oz margin |
| NPTH hole | 0.5mm min | |
| Silkscreen line / text height | 0.15mm / 1.0mm min | matches JLCPCB's minimum legible silkscreen |
| Copper-to-board-edge | 0.3-0.5mm min | JLCPCB minimum is 0.2mm; kept conservative |

Previously (before this pass) `.kicad_dru` had its own copy of the 20A power-net rule,
independently of the net classes in §3 — it had drifted: it referenced a `PHASE` net that
doesn't exist in this design (should have been `VS_A`/`VS_B`), still included `GND` (see §3 for
why that's wrong), and assumed 1oz copper. Removed rather than fixed in place, since keeping the
power-net width/clearance mandate in one place (net classes) is what avoids this kind of drift
recurring — `.kicad_dru` now only carries fab-capability floors that don't depend on which net.

Sourced from JLCPCB's [PCB Capabilities page](https://jlcpcb.com/capabilities/pcb-capabilities)
plus community-verified 2oz-specific guidance, current as of 2026-08; JLCPCB's own gerber-upload
checker is the final word before ordering, and per-part pricing/surcharge tiers weren't verified
precisely here (that page's exact cutoffs shift over time) — treat the "stays in the cheap tier"
framing above as a reasonable rule of thumb, not a guarantee.

## 5. Tooling
- **KiCad CLI**: `C:\Program Files\KiCad\10.0\bin\kicad-cli.exe`
- **Vendored Library**: `hardware\KiCad\libraries\easyeda2kicad\` (16 parts not available via PCM — symbols/footprints/3D models fetched with the `easyeda2kicad` tool, registered in `sym-lib-table`/`fp-lib-table`).
- **JLCPCB Passives**: install the `CDFER/JLCPCB-Kicad-Library` package per-machine via KiCad's Plugin & Content Manager — not vendored into the repo.
- **Component swap script**: `hardware/scripts/swap_part.py` — vendors a new LCSC part and swaps it into a schematic (and optionally the PCB), handling the reformatting/UUID/net-preservation details that are easy to get wrong by hand (and have been, repeatedly). Doesn't touch STANDARDS.md/CALCULATIONS.typ or run ERC/DRC — update the docs and open KiCad to check the result yourself, same as always. See the script's own docstring for usage and its current limitations (simple passives/2-terminal parts, legacy `module`-format footprints only).

## 6. Working with Claude

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
  the operating envelope (MOSFET Vds/Rds(on)/Qg vs. 200kHz switching losses, inductor Isat/DCR vs.
  the 20A continuous rating, TVS clamping voltage vs. the 80V rail margin, sensor bandwidth vs. the
  ~1.5kHz control loop), checks LCSC/JLCPCB stock/package availability, and documents the
  derivation in `CALCULATIONS.typ` — not just the final part number.
- **DRC/ERC enforcement**: this is a people job. Claude does not run `kicad-cli` DRC/ERC, proactively
  or on request — board correctness review stays with the user. The "no merge without a clean
  report" mandate above is checked by the user, not by Claude.
- **Change mechanics**: hardware edits (this file, `CALCULATIONS.typ`, KiCad files) go through a
  `hardware/<name>` branch and PR, per `CLAUDE.md` — never committed straight to `main`. Example:
  `hardware/fix-resistor-values`.
