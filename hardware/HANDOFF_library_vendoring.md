# Handoff: finish vendoring the easyeda2kicad component library

Scratch working notes for picking this up on the machine that has the original
KiCad component library. Delete this file once the work below is done and merged
— it's a handoff note, not permanent documentation.

## Context

The KiCad project (`hardware/KiCad/`) references two separate, unrelated symbol
sources that neither exist in this repo nor resolve on a fresh machine:

1. **`JLCPCB-*` lib_ids** (e.g. `JLCPCB-Resistors:0402,510Ω`) — come from the
   **CDFER/JLCPCB-Kicad-Library** community package, installable via KiCad's
   Plugin & Content Manager (search "JLCPCB", Libraries tab). This one is fine
   to just install per-machine via PCM; it's not vendored into the repo.
2. **`easyeda2kicad:*` lib_ids** (~17 specific ICs/connectors/passives, e.g.
   `easyeda2kicad:STM32F072RBT6`) — generated one-at-a-time by the separate
   `easyeda2kicad` Python tool (`pip install easyeda2kicad`) from LCSC part
   numbers. This is **not** installable via PCM and has no bulk source — it only
   ever existed as local files on whichever machine ran the tool. This is the
   one that needs vendoring into the repo so the project opens clean anywhere.

## What's already done (this session, other machine)

All 17 LCSC part numbers were already identified with certainty by reading the
`LCSC Part` / `Datasheet` properties already embedded in the schematic/PCB files
themselves (no guessing) — see `git log` on branch `docs/hardware-collab-workflow`
and the PR #3 description for how. Also found: `hardware/STANDARDS.md`'s
documented LCSC number for `CC6937S8-3FB020` (C5295991) was itself stale — the
schematic's own embedded value is the correct one below.

| Part | LCSC # |
|---|---|
| `STM32F072RBT6` | C46046 |
| `SY8120B1ABC` | C88474 |
| `SCT2A25STER` | C5124114 |
| `CC6937S8-3FB020` | **C41421919** (not C5295991 — STANDARDS.md is stale) |
| `BSC030N08NS5` | C501507 |
| `IRS21867STRPBF` | C52290 |
| `LKML2502A331MF` | C443153 (330µF leaded electrolytic, not an inductor despite the sheet name) |
| `CPEX3222L-470MC-RS` | C53145486 |
| `APS0650M470A` | C47327224 |
| `TCC1210X7R225K101MT` | C5449052 |
| `FTC201610S4R7MBCA` | C5832346 |
| `B4B-XH-A` | C144395 |
| `TYPE-C16PIN2MD` | C2765186 |
| `XT60PW-M` | C98732 |
| `2541WV-08P` | C5383116 |
| `PZ254V-11-02P` | C492401 |
| `PZ254V-11-04P` | C2691448 |

## Steps to finish

1. **Pull latest `main`** first (PR #3, "docs: hardware collaboration workflow +
   MOSFET datasheet", should be merged or mergeable — has the corrected git
   workflow docs and the `BSC030N08NS5` datasheet).

2. **Regenerate the library**:
   ```
   pip install easyeda2kicad
   easyeda2kicad --full --project-relative --overwrite \
     --lcsc_id C46046 C88474 C5124114 C41421919 C501507 C52290 C443153 \
               C53145486 C47327224 C5449052 C5832346 C144395 C2765186 \
               C98732 C5383116 C492401 C2691448 \
     --output "<repo>/hardware/KiCad/libraries/easyeda2kicad/easyeda2kicad.kicad_sym"
   ```
   Note: `--output` must be an **absolute path** — a relative path crashes the
   tool with a `relative_to` ValueError (confirmed bug in easyeda2kicad 1.0.1).

   On the sandboxed machine this session ran on, **16 of 17 succeeded** but
   `C41421919` (`CC6937S8-3FB020`) consistently failed with `HTTP 403 Forbidden`
   from the EasyEDA API (not a rate limit — retried after a delay, same result).
   This machine may not hit the same block. If it succeeds here, great, skip to
   step 4. If it still 403s, use step 3 instead.

3. **If `CC6937S8-3FB020` still 403s**, its symbol and footprint geometry is
   already fully cached in the project files themselves (from when it worked
   previously) and can be extracted directly instead of calling the API:
   - Symbol: `hardware/KiCad/2_Buck-Boost Stuff.kicad_sch`, the
     `(symbol "easyeda2kicad:CC6937S8-3FB020" ...)` block (grep for it — it was
     around line 5305 this session, may drift). Copy the whole block into
     `easyeda2kicad.kicad_sym`, and rename just the top-level symbol name from
     `"easyeda2kicad:CC6937S8-3FB020"` to `"CC6937S8-3FB020"` (the prefix is
     only used for eeschema's schematic-local cache, not in a standalone
     library file).
   - Footprint: `hardware/KiCad/ProPrj_STM32 MPPT_2024-07-25_15-40-35_2026-05-30.kicad_pcb`,
     the `(footprint "easyeda2kicad:SOP-8_L4.9-W3.9-P1.27-LS6.0-BL" ...)` block
     (grep for it — around line 9250 this session). To turn it into a standalone
     `.kicad_mod` in `easyeda2kicad.pretty/`: drop the top-level `(at X Y ROT)`,
     `(uuid ...)`, `(path ...)`, `(sheetname ...)`, `(sheetfile ...)` — pad and
     graphic-item coordinates underneath are already footprint-local, no
     recalculation needed — and drop each pad's `(net "...")` line (board-instance
     only, doesn't belong in a library footprint). Match the file format/style
     already used by the sibling files in `easyeda2kicad.pretty/` (legacy
     `(module ...)` s-expression style, not the newer `(footprint ...)` style —
     KiCad 10 reads both).
   - 3D model: genuinely not recoverable from this repo (never stored as text,
     only referenced by path) — if the API is blocked here too, this one part
     will just be missing its 3D preview. Cosmetic only; doesn't affect DRC or
     fab. Try LCSC's product page for C41421919 directly (a "3D model" download
     button, separate from the EasyEDA API) as a last resort.

4. **Register the library** in the project (`hardware/KiCad/sym-lib-table` and a
   new `hardware/KiCad/fp-lib-table` — the latter doesn't exist yet, only
   `sym-lib-table` does): add an entry with nickname `easyeda2kicad`
   (matching the existing schematic lib_ids exactly, so nothing needs
   remapping) pointing at `${KIPRJMOD}/hardware/KiCad/libraries/easyeda2kicad/easyeda2kicad.kicad_sym`
   (and the `.pretty` folder for fp-lib-table).

5. **Fix the `${EASYEDA2KICAD}` 3D-model path mismatch**: every *already-placed*
   footprint instance in the `.kicad_pcb` references its 3D model via
   `${EASYEDA2KICAD}/easyeda2kicad.3dshapes/...` (a KiCad environment variable
   that only existed on the machine that placed them), not `${KIPRJMOD}` (which
   is what fresh `--project-relative` output uses). Rather than requiring every
   future machine to configure that env var globally, add it as a **project-local
   text variable** instead — open `hardware/KiCad/ProPrj_STM32 MPPT_2024-07-25_15-40-35_2026-05-30.kicad_pro`
   (JSON) and add to its `"text_variables"` object (currently `{}`):
   ```json
   "text_variables": {
     "EASYEDA2KICAD": "${KIPRJMOD}/hardware/KiCad/libraries/easyeda2kicad"
   }
   ```
   KiCad resolves project text variables the same way as environment variables
   in file-path fields, so this makes every existing placement's 3D model
   resolve on any machine without machine-level setup.

6. **Verify**: open the project in KiCad, confirm no "library not found"
   warnings, run DRC/ERC (`kicad-cli`, path in `STANDARDS.md`) for a clean report.

7. **Commit on a `hardware/vendor-easyeda2kicad-library` branch**, push, open a
   PR. Note in the PR description that hardware-file review before merge is
   expected per `CLAUDE.md`.

## Also pending (logged as tasks this session, may not carry over — recreate if needed)

- Fix `STANDARDS.md`'s stale MOSFET entry: documented as `BRCS030N10SHRA`,
  schematic/user-confirmed actual part is `BSC030N08NS5` (C501507).
- Fill in `STANDARDS.md`'s Hardware Universe table with the ~13 parts from the
  table above that it's currently missing entirely.

## What NOT to bring over from the other machine's checkout

The machine this was drafted on has, uncommitted and intentionally not pushed:
- `hardware/KiCad/libraries/` — a partial (16/17) attempt at the same vendoring
  work above. Not pushed since the real library should come from the correct
  source machine instead of a rebuilt approximation.
- Modifications to the three root `.kicad_pro`/`.kicad_prl`/`.kicad_sch` files —
  these were touched by KiCad/PCM itself just from opening the project on that
  machine (recently-used lists, plugin registration), not intentional design
  changes. Don't merge these in from that machine.
