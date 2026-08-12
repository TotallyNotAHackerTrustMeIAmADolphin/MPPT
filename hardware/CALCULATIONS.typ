#set document(title: "openMPPT Hardware Design Calculations", author: "openMPPT project")
#set page(paper: "a4", numbering: "1", margin: 2.2cm)
#set text(font: "New Computer Modern", size: 10.5pt)
#set heading(numbering: "1.")
#show link: underline

#align(center)[
  #text(size: 18pt, weight: "bold")[openMPPT Hardware Design Calculations]

  #text(size: 10pt, fill: gray)[Theoretical basis and calculations for component selection]
]

#v(0.5cm)

= Power Stage Parameters

#table(
  columns: (auto, auto, auto, 1fr),
  align: (left, center, center, left),
  table.header([*Parameter*], [*Symbol*], [*Value*], [*Notes*]),
  [Switching Frequency], [$f_"sw"$], [100 kHz], [Defined in `mppt.h` (`TIMER_PERIOD = 240`)],
  [Maximum Input Voltage], [$V_"in,max"$], [80 V], [Hardware limit],
  [Maximum Output Current], [$I_"out,max"$], [20 A], [Safety limit],
  [Nominal System Voltage], [$V_"sys"$], [12V / 24V / 48V], [Target batteries],
  [Gate Drive Voltage], [$V_"drive"$], [10 V], [Optimized for IRS21867 thermals (v1.3)],
)

#block(
  fill: rgb("#fff4e5"),
  inset: 10pt,
  radius: 4pt,
  width: 100%,
)[
  *Planned change: 100kHz → 200kHz main switching frequency.* Not yet implemented —
  `TIMER_PERIOD` in `mppt.h` (currently 240) and the table above still reflect 100kHz.
  `setup_cubemx_env_auto.py` already derives `TIMER_PERIOD` from `MPPT.ioc` and `main.c`'s
  PWM limits/dead-bands are dynamic, so the firmware side should follow from changing the
  timer config. See @sec-200khz for the full component re-check.
]

= Gate Drive & Auxiliary Power Optimization (v1.3)

*Primary Aux Supply (10V) - SCT2A25:*
Replaces the unstable 12V XL7005A with a robust 100V-rated step-down (SCT2A25).
- Output is tuned to *10V* instead of 12V.
- *Feedback Divider*: $V_"FB" = 1.2"V"$
  - $V_"OUT" = 1.2 times (1 + R_"up"/R_"down")$
  - Using E96 series: $R_"up" = 110 "k"Omega$, $R_"down" = 15 "k"Omega arrow.r V_"OUT" = 1.2 times (1 + 110/15) = 10.0"V"$
- *Switching Frequency*: Fixed at 300kHz.
- *Inductor* ($L_1$): For 10V out, a standard *33µH or 47µH* inductor (e.g., 6x6mm shielded) is suitable.
- *UVLO Divider*: To set start at ~12.5V: $R_"UVLO_TOP" = 430 "k"Omega$, $R_"UVLO_BOT" = 47 "k"Omega$ (Both are E24).
- *Bootstrap Diode*: *US1M* (1000V, 1A Ultrafast, $t_"rr" <= 75"ns"$).

*Secondary Aux Supply (3.3V Logic) - SY8120:*
- Uses a "Cascaded Buck" architecture: 10V $arrow.r$ Tiny Sync Buck (SY8120) $arrow.r$ 3.3V.
- *Feedback Divider*: $V_"FB" = 0.6"V"$
  - $V_"OUT" = 0.6 times (1 + R_"up"/R_"down")$
  - Using E24 series: $R_"up" = 68 "k"Omega$, $R_"down" = 15 "k"Omega arrow.r V_"OUT" = 0.6 times (1 + 68/15) = 3.32"V" approx 3.3"V"$
- *Switching Frequency*: Fixed at 500kHz.
- *Inductor* ($L$): *4.7µH* (recommended for 3.3V out).

= Transient Voltage Suppression (TVS) Strategy

*Primary Bus Protection (80V):*
- *Component*: *5.0SMDJ85CA* (Bidirectional, 5kW).
- *Rationale*: Standoff ($V_"RWM"$) of 85V ensures no leakage at 80V. 5kW rating handles massive inductive kickback and solar surges.
- *Placement*: Immediately adjacent to VIN/VOUT XT60 connectors.

*Gate Drive Protection (10V):*
- *Component*: *H12VH22U* (6kW Surge).
- *Rationale*: 12V standoff ensures invisibility at 10V rail. Protects sensitive IRS21867 drivers (25V max) from regulator failure.

*Logic Protection (3.3V):*
- *Component*: *H3V3L06B* (ESD).
- *Rationale*: 3.3V standoff is required because STM32 Absolute Max is only 4.0V. Provides last line of defense against ESD and noise.

= Inductor Sizing (Main Power Stage) <sec-inductor>

Targeting a peak-to-peak ripple current ($Delta I_L$) of 20% of $I_"out,max"$ (*4.0 A*).

*Buck Mode Worst-Case:* (50% Duty Cycle, e.g., 80V in, 40V out)

$ L_"buck" = frac(V_"out" times (V_"in" - V_"out"), Delta I_L times f_"sw" times V_"in") $

At $f_"sw" = 100"kHz"$: $L_"buck" = frac(40 times 40, 4.0 times 100000 times 80) = *50.0 µ"H"*$

*v1.3 Component Selection:*
Target inductor size: *33 µH to 47 µH*.
- *CRITICAL*: Saturation Current ($I_"sat"$) *MUST be > 25A*. Using a lower $I_"sat"$ (like 11A) will cause magnetic collapse and MOSFET destruction.

= Bulk Capacitor Selection

Targeting an output voltage ripple ($Delta V_"out"$) of *< 100mV*.

$ "ESR"_"max" = frac(Delta V_"out", Delta I_L) = frac(0.1"V", 4.0"A") = *25 m Omega* $

*v1.3 Capacitor Strategy:*
- Use *4x 330µF 100V Low-ESR Electrolytic* (e.g., Ymin LKML series) in parallel.
- *Total Bank ESR*: $approx 11.75 m Omega$ (Passes 25mΩ limit).
- *Total Ripple Capacity*: $approx 8.5 "A"$ (Safe for 20A operation).

= Voltage Divider & ADC Scaling

*Voltage Sensing LPF (v1.3 Optimized):*
- $R_"top" = 200 "k"Omega$, $R_"bottom" = 4.7 "k"Omega$.
- $R_"th" approx 4.59 "k"Omega$
- $C_"filter" = 10 "nF"$
- $f_c = frac(1, 2 pi times 4590 times 10 times 10^(-9)) approx 3.47 "kHz"$
- _Evaluation_: Balanced for high-speed tracking and noise rejection.

= Current Sense (CC6937)

- *Type*: Isolated Hall Effect.
- *Sensitivity*: Check variant (e.g., 66mV/A for 20A).
- *Range*: ±20A (Full scale $approx 2.97"V"$ on 3.3V ADC).
- *VREF*: 100nF bypass to GND required.

= 200kHz Migration: Component Re-Check <sec-200khz>

This section re-verifies the parts already selected for 100kHz operation against a
doubled switching frequency, using values pulled directly from the vendored datasheets
(not assumed). Nothing here has been implemented in firmware or hardware yet — this is
the pre-check called for by the `STANDARDS.md` "Part selection & calculations" mandate
before the `ROADMAP_V1.3.md` Phase 4 checklist item is closed.

== Main Switching MOSFETs (Q1-Q4, BSC030N08NS5, C501507)

Source: Infineon `BSC030N08NS5` datasheet, Rev. 2.3, 2019-10-31.

#table(
  columns: (auto, auto, 1fr),
  align: (left, center, left),
  table.header([*Parameter*], [*Value*], [*Condition*]),
  [$R_"DS(on)"$], [2.6 mΩ typ / 3.0 mΩ max], [$V_"GS"$=10V, $I_D$=50A],
  [$Q_g$ (total gate charge)], [61 nC typ / 76 nC max], [$V_"DD"$=40V, $I_D$=50A, $V_"GS"$=0→10V],
  [$t_r$ (turn-on rise)], [12 ns typ], [$V_"DD"$=40V, $V_"GS"$=10V, $I_D$=50A, $R_"G,ext"$=3Ω],
  [$t_f$ (turn-off fall)], [13 ns typ], [same],
  [$C_"oss"$], [700 pF typ / 910 pF max], [$V_"DS"$=40V, f=1MHz],
  [$R_"th JA"$ (PCB only, no heatsink)], [50 K/W], [6 cm² copper, 1-layer, natural convection],
  [$T_"j,max"$], [150 °C], [—],
)

*Conduction loss* (frequency-independent):

$ P_"cond" = I_"rms"^2 times R_"DS(on),max" = 20^2 times 0.0030 approx *1.2 "W"* $

*Switching loss* (linear-approximation model, worst case $V_"DS" = 80"V"$, $I_D = 20"A"$):

$ P_"sw" = frac(1,2) times V_"DS" times I_D times (t_r + t_f) times f_"sw" $

#table(
  columns: (1fr, auto, auto),
  align: (left, center, center),
  table.header([], [*100 kHz (current)*], [*200 kHz (proposed)*]),
  [$P_"sw"$ per hard-switched MOSFET], [2.0 W], [4.0 W],
  [$P_"cond"$ per MOSFET], [1.2 W], [1.2 W (unchanged)],
  [*Total est. loss*], [*≈3.2 W*], [*≈5.2 W*],
)

#block(
  fill: rgb("#ffe5e5"),
  inset: 10pt,
  radius: 4pt,
  width: 100%,
)[
  *Thermal flag.* With PCB-only cooling ($R_"th JA"$=50 K/W, no heatsink/thermal vias),
  the datasheet's own max package dissipation at $T_A$=25°C is
  $P_"tot" = (150-25)/50 = *2.5 "W"*$ — already below the ≈3.2W estimate at *100kHz*,
  let alone ≈5.2W at 200kHz. This assumes worst-case $V_"DS"$/$I_D$ coincidence and a
  simplified switching-loss model (real loss depends on which of Q1-Q4 is hard-switched
  vs. synchronous-rectifying in the actual buck-boost topology, and gate resistance/drive
  strength affect $t_r$/$t_f$ in-circuit). This is exactly what `ROADMAP_V1.3.md` Phase 3's
  open item ("20A Thermal Design: widen high-power traces, implement massive thermal via
  grid") already anticipates — that item should land *before* or *alongside* the 200kHz
  switch, not after. The bottom-side $R_"th JC"$ path (0.5-0.9 K/W typ/max) is dramatically
  better than the PCB-only path and is what a proper thermal via array would unlock.
]

*Gate drive loss* (in the driver/gate resistor, not the MOSFET junction — informational):

$ P_"gate" = Q_g times V_"GS" times f_"sw" = 76 times 10^(-9) times 10 times f_"sw" $

At 100kHz: 76mW/MOSFET. At 200kHz: 152mW/MOSFET. Not a concern for the driver IC's
thermal budget either way.

== Gate Driver (IRS21867STRPBF, C52290)

Source: Infineon `IRS21867S` datasheet v01.00.

#table(
  columns: (auto, auto, 1fr),
  align: (left, center, left),
  table.header([*Parameter*], [*Value*], [*Condition*]),
  [$t_"on"$ / $t_"off"$ propagation delay], [170 ns typ / 250 ns max], [$V_"CC"=V_"BS"$=15V, $C_L$=1000pF],
  [MT (delay matching, $|t_"on" - t_"off"|$)], [35 ns max], [—],
  [$t_r$ / $t_f$ (driver output)], [22/18 ns typ, 38/30 ns max], [—],
  [Output source/sink current], [4.0 A typ], [—],
)

At 200kHz the switching period is 5.0µs vs. 10µs at 100kHz. The 170-250ns propagation
delay grows from ~2.5% to ~5% of the period — worth including explicitly in dead-time
budgeting, but well within normal margins; *not* a hard blocker on 200kHz operation.
Output drive current (4.0A) is unchanged and still comfortably swings $Q_g$=76nC in the
tens-of-ns range required. No datasheet-stated maximum switching frequency applies here.

== Main Inductor (currently L4, FC-SE2822-150M, C46553544)

Re-deriving @sec-inductor's formula at 200kHz:

$ L_"buck" (200"kHz") = frac(40 times 40, 4.0 times 200000 times 80) = *25.0 µ"H"* $

This *halves* the 100kHz target (50µH → 25µH), which also shifts the "target range" from
33-47µH down toward roughly *16.5-23.5µH* if the same 20%-ripple-target methodology and
25A $I_"sat"$ margin are kept.

#block(
  fill: rgb("#e5f3ff"),
  inset: 10pt,
  radius: 4pt,
  width: 100%,
)[
  *Ties back to the open L4 question in `STANDARDS.md`.* L4 (`FC-SE2822-150M`) was flagged
  there because its part-name convention implies ≈15µH, well under the 33-47µH target for
  *100kHz*. At *200kHz*, that same 15µH is much closer to (if still slightly under) the new
  ≈16.5-23.5µH target — the frequency change would substantially close that gap rather than
  widen it. This is *not* confirmation the two were chosen together on purpose; it's a
  reason to verify the inductance value from the datasheet/markings rather than resolve it
  by coincidence. $I_"sat"$ still needs independent confirmation against the 20-25A
  requirement regardless of frequency.
]

== Summary

#table(
  columns: (1fr, auto),
  align: (left, left),
  table.header([*Item*], [*Verdict at 200kHz*]),
  [MOSFETs (BSC030N08NS5)], [Electrically fine (Vds/Rds(on)/Qg headroom unchanged), but switching loss ≈doubles — thermal design (ROADMAP Phase 3) must land first],
  [Gate driver (IRS21867STRPBF)], [No concern — propagation delay and drive current have ample margin],
  [Main inductor (L4)], [Required inductance halves to ≈25µH — worth re-deriving the target range and confirming L4's actual value/Isat before committing],
  [Bulk caps / TVS / voltage sensing], [Frequency-independent, no re-check needed],
)
