#!/usr/bin/env python3
"""
Swap a KiCad component (schematic symbol + optional PCB footprint) for a
different LCSC part, pulling the new symbol/footprint/3D model via
easyeda2kicad into the project's vendored library.

Handles the mechanical parts of a part swap that are easy to get wrong by
hand (and have been, repeatedly, in this project's history): reformatting
the raw easyeda2kicad property blocks into the schematic's embedded
lib_symbols style (which needs `show_name`/`do_not_autoplace` added - KiCad
defaults differ without them), preserving the placed instance's
position/UUID/pin-UUIDs and its Datasheet/Manufacturer/MPN/LCSC Part
properties, converting the new footprint (legacy `module` format) into the
project's modern `footprint` format while preserving position, the
schematic-symbol `path` link, and pad-to-net mapping by pad number, and
keeping the file's original line-ending style (this repo uses CRLF).

The symbol's actual graphics (coil arcs, diode triangle, whatever the part
looks like) are carried over from the vendored library near-verbatim, just
re-indented - not hand-modeled - so this doesn't silently turn every part
into a generic circle.

Usage:
    python hardware/scripts/swap_part.py \\
        --ref L2 \\
        --old-lib-id easyeda2kicad:APS0650M470A \\
        --new-lcsc C52196367 \\
        --sch hardware/KiCad/power.kicad_sch \\
        --pcb hardware/KiCad/openMPPT_v1.3.kicad_pcb

Only handles simple passives/2-terminal parts placed with
`(lib_id "easyeda2kicad:<name>")` in the project-relative `easyeda2kicad`
library, with a legacy `(module ...)` footprint (that's what
easyeda2kicad has produced for every part vendored so far - if a future
part comes out in the modern `(footprint ...)` format instead, the script
says so and you insert that one by hand).

Does NOT touch STANDARDS.md/CALCULATIONS.typ - update those by hand
afterward, same as always. Does NOT run kicad-cli ERC/DRC - that stays a
people job, per hardware/STANDARDS.md.
"""

import argparse
import math
import re
import subprocess
import sys
import uuid
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
LIBRARY_DIR = REPO_ROOT / "hardware" / "KiCad" / "libraries" / "easyeda2kicad"


def u():
    return str(uuid.uuid4())


def detect_newline(path: Path) -> bytes:
    return b"\r\n" if b"\r\n" in path.read_bytes() else b"\n"


def write_preserving_newline(path: Path, text: str, newline: bytes):
    data = text.encode("utf-8").replace(b"\r\n", b"\n")
    if newline == b"\r\n":
        data = data.replace(b"\n", b"\r\n")
    path.write_bytes(data)


def vendor_part(lcsc_id: str) -> str:
    """Run easyeda2kicad and return the vendored symbol's name, parsed
    straight from its own stdout (not guessed from library file order)."""
    LIBRARY_DIR.mkdir(parents=True, exist_ok=True)
    sym_path = LIBRARY_DIR / "easyeda2kicad.kicad_sym"
    cmd = [
        "easyeda2kicad", "--full", "--project-relative", "--overwrite",
        "--lcsc_id", lcsc_id,
        "--output", str(sym_path),
    ]
    print(f"$ {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    # easyeda2kicad logs everything (INFO/WARNING/"Symbol name :") to stderr;
    # stdout only gets the final "-- easyeda2kicad.py vX --" banner.
    print(result.stderr)
    if result.returncode != 0:
        sys.exit(f"easyeda2kicad failed for {lcsc_id}")
    m = re.search(r"Symbol name\s*:\s*(\S+)", result.stderr)
    if not m:
        sys.exit(f"could not find 'Symbol name :' in easyeda2kicad output for {lcsc_id}")
    return m.group(1)


# ---------------------------------------------------------------------------
# Schematic symbol swap
# ---------------------------------------------------------------------------

def parse_vendored_symbol(name: str) -> dict:
    """Extract fields + raw sub-symbol block for `name` from the vendored
    easyeda2kicad.kicad_sym (the tool's own multi-line/space-indented
    output format, distinct from what's embedded in a .kicad_sch)."""
    text = (LIBRARY_DIR / "easyeda2kicad.kicad_sym").read_text(encoding="utf-8")
    m = re.search(r'\(symbol "%s"' % re.escape(name), text)
    if not m:
        sys.exit(f"symbol '{name}' not found in vendored library - did vendoring succeed?")
    start = m.start()
    end = text.find('\n(symbol "', m.end())
    if end == -1:
        end = len(text)
    block = text[start:end]

    def prop(key):
        pm = re.search(r'\(property\s*"%s"\s*"([^"]*)"' % re.escape(key), block)
        return pm.group(1) if pm else ""

    footprint = prop("Footprint").split(":", 1)[-1]

    sub_m = re.search(r'\(symbol "%s_0_1"' % re.escape(name), block)
    if not sub_m:
        sys.exit(f"could not find '{name}_0_1' graphics sub-block")
    sub_start = sub_m.start()
    # matching close paren for the sub-symbol: walk depth from sub_start
    depth = 0
    i = sub_start
    while True:
        if block[i] == "(":
            depth += 1
        elif block[i] == ")":
            depth -= 1
            if depth == 0:
                break
        i += 1
    sub_block = block[sub_start:i + 1]

    pins = []
    for pm in re.finditer(
        r'\(pin \w+ line\s*\(at ([\d.\-]+) ([\d.\-]+) (\d+)\).*?\(number "(\w+)"',
        sub_block, re.S,
    ):
        x, y, rot, num = pm.groups()
        pins.append((num, x, y, rot))

    return {
        "name": name,
        "footprint": footprint,
        "datasheet": prop("Datasheet"),
        "manufacturer": prop("Manufacturer"),
        "mpn": prop("MPN"),
        "lcsc": prop("LCSC Part"),
        "pins": pins,
        "sub_block_raw": sub_block,
    }


def _parse_sexpr_list(tokens: list, i: int):
    """Parse tokens[i:] (i pointing just past an opening '(') into a list
    of elements (strings for atoms, nested lists for sub-expressions),
    returning (elements, index_after_closing_paren). A real parse tree,
    not manual depth-counting on a flat stream - that's what produced
    paren-unbalanced output before."""
    elements = []
    while tokens[i] != ")":
        if tokens[i] == "(":
            sub, i = _parse_sexpr_list(tokens, i + 1)
            elements.append(sub)
        else:
            elements.append(tokens[i])
            i += 1
    return elements, i + 1


def _render_sexpr(elements, depth: int, base_tabs: int) -> str:
    """Render a parsed element list. A list with no nested sub-lists is a
    leaf ('(start -2.03 -0.01)') and stays on one line. Otherwise, leading
    atoms (the "tag", e.g. "symbol", "NAME") stay on the opening-paren
    line - matching this file's own style, e.g. `(property "Reference"
    "L"` - and the remaining elements (starting from the first nested
    list) each get their own line."""
    is_leaf = not any(isinstance(e, list) for e in elements)
    indent = "\t" * (base_tabs + depth)
    if is_leaf:
        return indent + "(" + " ".join(elements) + ")"
    split = 0
    while split < len(elements) and not isinstance(elements[split], list):
        split += 1
    head = " ".join(elements[:split])
    lines = [indent + "(" + (head if head else "")]
    for e in elements[split:]:
        if isinstance(e, list):
            lines.append(_render_sexpr(e, depth + 1, base_tabs))
        else:
            lines.append("\t" * (base_tabs + depth + 1) + e)
    lines.append(indent + ")")
    return "\n".join(lines)


def reindent_sexpr(raw: str, base_tabs: int) -> str:
    """Re-indent a raw (space-indented, KiCad-library-style) S-expression
    block to tab-indentation at `base_tabs`, without touching its semantic
    content - KiCad doesn't care about whitespace style, this is purely
    for readability in a diff/editor. Parses into a real tree and
    re-renders it, which is correct by construction (every list this
    function builds is closed exactly where it was opened) rather than
    tracking paren depth by hand across a flat token stream."""
    tokens = re.findall(r'"[^"]*"|\(|\)|[^\s()]+', raw)
    assert tokens[0] == "(", "expected raw block to start with '('"
    elements, end = _parse_sexpr_list(tokens, 1)
    assert end == len(tokens), f"trailing tokens after parse ({len(tokens) - end} left over) - malformed input?"
    return _render_sexpr(elements, depth=0, base_tabs=base_tabs)


def build_schematic_master_block(info: dict, ref_prefix: str) -> str:
    """Emit a lib_symbols block in the project's embedded style: properties
    get real semantic fields added (show_name/do_not_autoplace), graphics
    are carried over from the vendored library, re-indented."""
    name = info["name"]

    def prop_block(key, value, at, hide):
        hide_line = "\t\t\t\t(hide yes)\n" if hide else ""
        return (
            f'\t\t\t(property "{key}" "{value}"\n'
            f'\t\t\t\t(at {at})\n'
            f'\t\t\t\t(show_name no)\n'
            f'\t\t\t\t(do_not_autoplace no)\n'
            f'{hide_line}'
            f'\t\t\t\t(effects\n'
            f'\t\t\t\t\t(font\n'
            f'\t\t\t\t\t\t(size 1.27 1.27)\n'
            f'\t\t\t\t\t)\n'
            f'\t\t\t\t)\n'
            f'\t\t\t)\n'
        )

    props = ""
    props += prop_block("Reference", ref_prefix, "0 5.08 0", False)
    props += prop_block("Value", name, "0 -5.08 0", False)
    props += prop_block("Footprint", f"easyeda2kicad:{info['footprint']}", "0 -7.62 0", True)
    props += prop_block("Datasheet", info["datasheet"], "0 -10.16 0", True)
    props += prop_block("Description", "", "0 0 0", True)
    props += prop_block("Manufacturer", info["manufacturer"], "0 -12.7 0", True)
    props += prop_block("MPN", info["mpn"], "0 -15.24 0", True)
    props += prop_block("LCSC Part", info["lcsc"], "0 -17.78 0", True)

    graphics = reindent_sexpr(info["sub_block_raw"], base_tabs=3)

    return (
        f'\t\t(symbol "easyeda2kicad:{name}"\n'
        f'\t\t\t(exclude_from_sim no)\n'
        f'\t\t\t(in_bom yes)\n'
        f'\t\t\t(on_board yes)\n'
        f'\t\t\t(in_pos_files yes)\n'
        f'\t\t\t(duplicate_pin_numbers_are_jumpers no)\n'
        f'{props}'
        f'{graphics}\n'
        f'\t\t\t(embedded_fonts no)\n'
        f'\t\t)'
    )


def swap_schematic(sch_path: Path, old_lib_id: str, info: dict, ref: str, keep_value: bool):
    newline = detect_newline(sch_path)
    text = sch_path.read_text(encoding="utf-8")
    old_name = old_lib_id.split(":", 1)[-1]
    new_name = info["name"]
    new_lib_id = f"easyeda2kicad:{new_name}"

    # capture the OLD instance's property values BEFORE any edits, scoped
    # to the block containing our target Reference, so multi-instance
    # symbols (e.g. two of the same part) don't cross-contaminate.
    ref_m = re.search(r'\(property "Reference" "%s"' % re.escape(ref), text)
    if not ref_m:
        sys.exit(f"Reference '{ref}' not found in {sch_path}")
    # Placed instances are `(symbol\n\t...(lib_id ...` - bare, no quoted name
    # right after "symbol" (that's what distinguishes them from named
    # lib_symbols master/sub-symbol definitions like `(symbol "NAME_x_y"`).
    # Match any number of leading tabs - this file's indentation isn't
    # uniform (L2's instance sits at 2 tabs, L3's at 1), so don't assume a
    # fixed depth. A plain (un-anchored) rfind() on a fixed-depth string
    # previously matched as a *substring* of a more deeply nested
    # sub-symbol definition elsewhere in the file - anchoring to line
    # start (\n) plus "any tabs, then literally '(symbol', then only
    # whitespace before the next newline" avoids both failure modes.
    inst_starts = [m.start() + 1 for m in re.finditer(r'\n\t+\(symbol[ \t]*\n', text)]
    inst_starts = [p for p in inst_starts if p <= ref_m.start()]
    if not inst_starts:
        sys.exit(f"could not find an enclosing placed-instance '(symbol' block before Reference '{ref}'")
    inst_start = inst_starts[-1]
    inst_end_m = re.search(r'\n\t+\(symbol', text[ref_m.start():])
    if not inst_end_m:
        sys.exit(f"could not find end of instance block for Reference '{ref}'")
    inst_end = ref_m.start() + inst_end_m.start()
    instance_block = text[inst_start:inst_end]

    old_vals = {}
    for key in ("Footprint", "Datasheet", "Manufacturer", "MPN", "LCSC Part", "Value"):
        pm = re.search(r'\(property "%s" "([^"]*)"' % re.escape(key), instance_block)
        old_vals[key] = pm.group(1) if pm else None

    if instance_block.count(f'(lib_id "{old_lib_id}")') == 0:
        sys.exit(f"instance for Reference '{ref}' doesn't reference lib_id '{old_lib_id}' - check --old-lib-id")

    # Some instances carry a `(lib_name "X")` override (KiCad's local-cache
    # disambiguation, e.g. after a name collision) - when present, the
    # master block is named "X", not "easyeda2kicad:<name>". Look for that
    # first; the plain lib_id-derived name is the common case.
    lib_name_m = re.search(r'\(lib_name "([^"]*)"\)', instance_block)
    master_key = lib_name_m.group(1) if lib_name_m else old_lib_id

    # 1. master lib_symbols block (shared across all instances of this part)
    marker = f'\t\t(symbol "{master_key}"'
    if marker not in text:
        sys.exit(f"master symbol block for '{master_key}' not found in {sch_path} "
                  f"(derived from {'lib_name override' if lib_name_m else 'lib_id'})")
    m_start = text.index(marker)
    m_end = text.find('\n\t\t(symbol "', m_start + 10)
    if m_end == -1:
        sys.exit("could not find end of master symbol block (unexpected library layout)")
    old_master = text[m_start:m_end]
    ref_prefix = ref.rstrip("0123456789")
    new_master = build_schematic_master_block(info, ref_prefix)
    text = text.replace(old_master, new_master, 1)
    print(f"  master block: {master_key} -> {new_name}"
          + (" (was under a lib_name override, now using the plain lib_id convention)" if lib_name_m else ""))

    # 2. this instance's lib_id + properties, replacing the OLD values we
    # captured above (works even with multiple instances of the same part,
    # since we only touch the specific instance_block region). Drop any
    # lib_name override - the fresh master block uses the plain convention.
    new_instance_block = instance_block.replace(f'(lib_id "{old_lib_id}")', f'(lib_id "{new_lib_id}")', 1)
    if lib_name_m:
        # regex, not a fixed-tab-count literal - this file's indentation
        # isn't uniform (see the instance-boundary comment above), so
        # match "whatever whitespace actually follows" instead of guessing
        new_instance_block = re.sub(
            r'\(lib_name "%s"\)\s*' % re.escape(lib_name_m.group(1)), '', new_instance_block, count=1)
    for key, new_val in [
        ("Footprint", f"easyeda2kicad:{info['footprint']}"),
        ("Datasheet", info["datasheet"]),
        ("Manufacturer", info["manufacturer"]),
        ("MPN", info["mpn"]),
        ("LCSC Part", info["lcsc"]),
    ]:
        old_val = old_vals.get(key)
        if old_val is None:
            print(f"  WARNING: instance had no '{key}' property to replace - check manually")
            continue
        new_instance_block = new_instance_block.replace(
            f'(property "{key}" "{old_val}"', f'(property "{key}" "{new_val}"', 1)

    if not keep_value and old_vals.get("Value") == old_name:
        new_instance_block = new_instance_block.replace(
            f'(property "Value" "{old_name}"', f'(property "Value" "{new_name}"', 1)
        print(f"  instance Value: {old_name} -> {new_name}")
    elif old_vals.get("Value") != old_name:
        print(f"  instance Value left as-is ({old_vals.get('Value')!r} - doesn't match old part "
              f"name '{old_name}', looks like a custom override)")

    # Content-based, not index-based: the master-block replacement above
    # already shifted every offset after it (old_master and new_master
    # differ in length), so inst_start/inst_end from before that edit no
    # longer point at the right place in the now-larger/smaller `text`.
    if text.count(instance_block) != 1:
        sys.exit(f"instance block for '{ref}' isn't uniquely findable after the master-block edit - "
                  "swap aborted rather than risk splicing at the wrong offset")
    text = text.replace(instance_block, new_instance_block, 1)
    write_preserving_newline(sch_path, text, newline)
    print(f"  instance properties updated for {ref}")
    return new_lib_id


# ---------------------------------------------------------------------------
# PCB footprint swap
# ---------------------------------------------------------------------------

def rotate_point(px, py, cx, cy, angle_deg):
    a = math.radians(angle_deg)
    dx, dy = px - cx, py - cy
    rx = dx * math.cos(a) - dy * math.sin(a)
    ry = dx * math.sin(a) + dy * math.cos(a)
    return round(cx + rx, 3), round(cy + ry, 3)


def parse_legacy_footprint(mod_text: str):
    """Parse a legacy `(module ...)` easyeda2kicad footprint into a
    format-agnostic list of graphics + pads. fp_arc's (start=center, end,
    angle) is converted to modern (start, mid, end) via real rotation
    math, not approximated."""
    graphics = []
    for m in re.finditer(
        r'\(fp_line \(start ([\d.\-]+) ([\d.\-]+)\) \(end ([\d.\-]+) ([\d.\-]+)\) \(layer ([\w.]+)\) \(width ([\d.]+)\)\)',
        mod_text,
    ):
        sx, sy, ex, ey, layer, width = m.groups()
        graphics.append(("line", sx, sy, ex, ey, layer, width))

    for m in re.finditer(
        r'\(fp_circle \(center ([\d.\-]+) ([\d.\-]+)\) \(end ([\d.\-]+) ([\d.\-]+)\) \(layer ([\w.]+)\) \(width ([\d.]+)\)\)',
        mod_text,
    ):
        cx, cy, ex, ey, layer, width = m.groups()
        graphics.append(("circle", cx, cy, ex, ey, layer, width))

    for m in re.finditer(
        r'\(fp_arc \(start ([\d.\-]+) ([\d.\-]+)\) \(end ([\d.\-]+) ([\d.\-]+)\) \(angle ([\d.\-]+)\) \(layer ([\w.]+)\) \(width ([\d.]+)\)\)',
        mod_text,
    ):
        cx, cy, ex, ey, angle, layer, width = m.groups()
        cxf, cyf, exf, eyf, af = map(float, (cx, cy, ex, ey, angle))
        end2 = rotate_point(exf, eyf, cxf, cyf, af)
        mid = rotate_point(exf, eyf, cxf, cyf, af / 2)
        graphics.append(("arc", ex, ey, str(mid[0]), str(mid[1]), str(end2[0]), str(end2[1]), layer, width))

    pads = []
    for m in re.finditer(
        r'\(pad (\d+) (smd|thru_hole) (\w+) \(at ([\d.\-]+) ([\d.\-]+)(?: ([\d.\-]+))?\) '
        r'\(size ([\d.]+) ([\d.]+)\)(?: \(roundrect_rratio ([\d.]+)\))? '
        r'\(layers ([^)]+)\)(?:\(drill oval ([\d.]+) ([\d.]+)\)|\(drill ([\d.]+)\))?\)',
        mod_text,
    ):
        num, kind, shape, x, y, rot, sx, sy, rratio, layers, doval1, doval2, drill = m.groups()
        pads.append({
            "num": num, "kind": kind, "shape": shape, "x": x, "y": y, "rot": rot or "0",
            "sx": sx, "sy": sy, "rratio": rratio, "layers": layers.split(),
            "drill_oval": (doval1, doval2) if doval1 else None,
            "drill": drill,
        })
    return graphics, pads


def emit_graphics(graphics) -> str:
    out = []
    for g in graphics:
        if g[0] == "line":
            _, sx, sy, ex, ey, layer, width = g
            out.append(
                f'\t\t(fp_line\n\t\t\t(start {sx} {sy})\n\t\t\t(end {ex} {ey})\n'
                f'\t\t\t(stroke\n\t\t\t\t(width {width})\n\t\t\t\t(type solid)\n\t\t\t)\n'
                f'\t\t\t(layer "{layer}")\n\t\t\t(uuid "{u()}")\n\t\t)'
            )
        elif g[0] == "circle":
            _, cx, cy, ex, ey, layer, width = g
            out.append(
                f'\t\t(fp_circle\n\t\t\t(center {cx} {cy})\n\t\t\t(end {ex} {ey})\n'
                f'\t\t\t(stroke\n\t\t\t\t(width {width})\n\t\t\t\t(type solid)\n\t\t\t)\n'
                f'\t\t\t(fill no)\n\t\t\t(layer "{layer}")\n\t\t\t(uuid "{u()}")\n\t\t)'
            )
        elif g[0] == "arc":
            _, sx, sy, mx, my, ex, ey, layer, width = g
            out.append(
                f'\t\t(fp_arc\n\t\t\t(start {sx} {sy})\n\t\t\t(mid {mx} {my})\n\t\t\t(end {ex} {ey})\n'
                f'\t\t\t(stroke\n\t\t\t\t(width {width})\n\t\t\t\t(type solid)\n\t\t\t)\n'
                f'\t\t\t(layer "{layer}")\n\t\t\t(uuid "{u()}")\n\t\t)'
            )
    return "\n".join(out)


def emit_pads(pads, net_map: dict) -> str:
    out = []
    for p in pads:
        net = net_map.get(p["num"], "")
        net_line = f'\n\t\t\t(net "{net}")' if net else ""
        layers_str = " ".join(f'"{l}"' for l in p["layers"])
        shape_extra = f'\n\t\t\t(roundrect_rratio {p["rratio"]})' if p["rratio"] else ""
        if p["drill_oval"]:
            drill_line = f'\n\t\t\t(drill oval {p["drill_oval"][0]} {p["drill_oval"][1]})'
        elif p["drill"]:
            drill_line = f'\n\t\t\t(drill {p["drill"]})'
        else:
            drill_line = ""
        at_line = f'(at {p["x"]} {p["y"]})' if p["rot"] == "0" else f'(at {p["x"]} {p["y"]} {p["rot"]})'
        out.append(
            f'\t\t(pad "{p["num"]}" {p["kind"]} {p["shape"]}\n'
            f'\t\t\t{at_line}\n'
            f'\t\t\t(size {p["sx"]} {p["sy"]})'
            f'{shape_extra}'
            f'{drill_line}\n'
            f'\t\t\t(layers {layers_str})'
            f'{net_line}\n'
            f'\t\t\t(pintype "unspecified")\n'
            f'\t\t\t(uuid "{u()}")\n'
            f'\t\t)'
        )
    return "\n".join(out)


def swap_pcb(pcb_path: Path, ref: str, footprint_name: str, info: dict):
    newline = detect_newline(pcb_path)
    text = pcb_path.read_text(encoding="utf-8")

    m = re.search(r'\(property "Reference" "%s"' % re.escape(ref), text)
    if not m:
        sys.exit(f"reference '{ref}' not found in {pcb_path}")
    start = text.rfind('\n\t(footprint "', 0, m.start())
    end = text.find('\n\t(footprint "', m.start())
    old_block = text[start:end]

    at_m = re.search(r'\(at ([\d.\-]+) ([\d.\-]+)(?: ([\d.\-]+))?\)', old_block)
    pos_x, pos_y, pos_r = at_m.group(1), at_m.group(2), at_m.group(3) or "0"
    path_m = re.search(r'\(path "([^"]*)"\)', old_block)
    path = path_m.group(1) if path_m else ""
    sheetname_m = re.search(r'\(sheetname "([^"]*)"\)', old_block)
    sheetfile_m = re.search(r'\(sheetfile "([^"]*)"\)', old_block)
    sheetname = sheetname_m.group(1) if sheetname_m else ""
    sheetfile = sheetfile_m.group(1) if sheetfile_m else ""
    value_m = re.search(r'\(property "Value" "([^"]*)"', old_block)
    old_value = value_m.group(1) if value_m else info["name"]

    old_pads = re.findall(r'\(pad "(\w+)"[^\n]*\n(?:[^\n]*\n){0,15}?[^\n]*\(net "([^"]*)"\)', old_block)
    net_map = dict(old_pads)
    if not net_map:
        print("  WARNING: could not extract pad-net mapping from old footprint - check manually")

    mod_path = LIBRARY_DIR / "easyeda2kicad.pretty" / f"{footprint_name}.kicad_mod"
    mod_text = mod_path.read_text(encoding="utf-8")
    if mod_text.lstrip().startswith("(footprint "):
        sys.exit(
            f"'{footprint_name}' is already in modern (footprint ...) format - "
            "this script only auto-converts legacy (module ...) format. "
            "Insert it by hand this time, or extend parse_legacy_footprint()."
        )
    graphics, pads = parse_legacy_footprint(mod_text)
    if len(pads) != len(net_map):
        print(f"  WARNING: old footprint had {len(net_map)} pads, new one has {len(pads)} - "
              "verify pad-net mapping by hand before trusting this swap")

    attr_m = re.search(r'\(attr (\w+)\)', mod_text)
    attr = attr_m.group(1) if attr_m else "smd"
    model_m = re.search(r'\(model "[^"]*/([^/"]+)"', mod_text)
    model_name = model_m.group(1).rsplit(".", 1)[0] if model_m else footprint_name
    rotate_m = re.search(r'\(rotate \(xyz ([\d.\-]+) ([\d.\-]+) ([\d.\-]+)\)\)', mod_text)
    rot_xyz = rotate_m.groups() if rotate_m else ("0", "0", "0")

    graphics_txt = emit_graphics(graphics)
    pads_txt = emit_pads(pads, net_map)

    new_block = f'''
\t(footprint "easyeda2kicad:{footprint_name}"
\t\t(layer "F.Cu")
\t\t(uuid "{u()}")
\t\t(at {pos_x} {pos_y}{" " + pos_r if pos_r != "0" else ""})
\t\t(property "Reference" "{ref}"
\t\t\t(at 0 -6.9 0)
\t\t\t(layer "F.SilkS")
\t\t\t(uuid "{u()}")
\t\t\t(effects (font (size 1 1) (thickness 0.15)))
\t\t)
\t\t(property "Value" "{old_value}"
\t\t\t(at 0 6.9 0)
\t\t\t(layer "F.Fab")
\t\t\t(uuid "{u()}")
\t\t\t(effects (font (size 1 1) (thickness 0.15)))
\t\t)
\t\t(property "Datasheet" "{info['datasheet']}"
\t\t\t(at 0 0 0)
\t\t\t(layer "F.Fab")
\t\t\t(hide yes)
\t\t\t(uuid "{u()}")
\t\t\t(effects (font (size 1.27 1.27)))
\t\t)
\t\t(property "LCSC Part" "{info['lcsc']}"
\t\t\t(at 0 0 0)
\t\t\t(layer "F.Fab")
\t\t\t(hide yes)
\t\t\t(uuid "{u()}")
\t\t\t(effects (font (size 1.27 1.27)))
\t\t)
\t\t(property "Manufacturer" "{info['manufacturer']}"
\t\t\t(at 0 0 0)
\t\t\t(unlocked yes)
\t\t\t(layer "F.Fab")
\t\t\t(hide yes)
\t\t\t(uuid "{u()}")
\t\t\t(effects (font (size 1 1) (thickness 0.15)))
\t\t)
\t\t(property "MPN" "{info['mpn']}"
\t\t\t(at 0 0 0)
\t\t\t(unlocked yes)
\t\t\t(layer "F.Fab")
\t\t\t(hide yes)
\t\t\t(uuid "{u()}")
\t\t\t(effects (font (size 1 1) (thickness 0.15)))
\t\t)
\t\t(path "{path}")
\t\t(sheetname "{sheetname}")
\t\t(sheetfile "{sheetfile}")
\t\t(units
\t\t\t(unit
\t\t\t\t(name "A")
\t\t\t\t(pins {" ".join(f'"{p["num"]}"' for p in pads)})
\t\t\t)
\t\t)
\t\t(attr {attr})
\t\t(duplicate_pad_numbers_are_jumpers no)
{graphics_txt}
{pads_txt}
\t\t(embedded_fonts no)
\t\t(model "${{KIPRJMOD}}/hardware/KiCad/libraries/easyeda2kicad/easyeda2kicad.3dshapes/{model_name}.wrl"
\t\t\t(offset (xyz 0 0 0))
\t\t\t(scale (xyz 1 1 1))
\t\t\t(rotate (xyz {rot_xyz[0]} {rot_xyz[1]} {rot_xyz[2]}))
\t\t)
\t)'''

    text = text[:start] + new_block + text[end:]
    write_preserving_newline(pcb_path, text, newline)
    print(f"  PCB: {ref} footprint -> {footprint_name}, {len(pads)} pads")
    print(f"  net mapping preserved: {net_map}")


# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", required=True, help="Reference designator, e.g. L2")
    ap.add_argument("--old-lib-id", required=True, help='e.g. easyeda2kicad:APS0650M470A')
    ap.add_argument("--new-lcsc", required=True, help="LCSC part number to vendor and swap in, e.g. C52196367")
    ap.add_argument("--sch", required=True, type=Path, help="Schematic file containing the part")
    ap.add_argument("--pcb", type=Path, help="Optional: also swap the PCB footprint")
    ap.add_argument("--keep-value", action="store_true",
                     help="Don't touch the instance's Value property even if it matches the old part name")
    args = ap.parse_args()

    print(f"Vendoring {args.new_lcsc}...")
    new_name = vendor_part(args.new_lcsc)

    print(f"Parsing vendored symbol '{new_name}'...")
    info = parse_vendored_symbol(new_name)

    print(f"Updating schematic {args.sch}...")
    new_lib_id = swap_schematic(args.sch, args.old_lib_id, info, args.ref, args.keep_value)

    if args.pcb:
        print(f"Updating PCB {args.pcb}...")
        swap_pcb(args.pcb, args.ref, info["footprint"], info)

    print("\nDone. This does NOT run ERC/DRC (people job, per STANDARDS.md) and does")
    print("NOT update STANDARDS.md/CALCULATIONS.typ - do that by hand, same as always.")
    print(f"New lib_id: {new_lib_id}")


if __name__ == "__main__":
    main()
