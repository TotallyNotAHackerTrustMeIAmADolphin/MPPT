# openMPPT Hardware Design Calculations

This document stores the theoretical basis and calculations for component selection in the openMPPT controller.

## 1. Power Stage Parameters
| Parameter | Symbol | Value | Notes |
| :--- | :--- | :--- | :--- |
| Switching Frequency | $f_{sw}$ | 100 kHz | Defined in `mppt.h` (`TIMER_PERIOD = 240`) |
| Maximum Input Voltage | $V_{in,max}$ | 80 V | Hardware limit |
| Maximum Output Current | $I_{out,max}$ | 20 A | Safety limit |
| Nominal System Voltage | $V_{sys}$ | 12V / 24V / 48V | Target batteries |
| Gate Drive Voltage | $V_{drive}$ | 10 V | Optimized for IRS21867 thermals (v1.3) |

## 2. Gate Drive & Auxiliary Power Optimization (v1.3)
**Primary Aux Supply (10V) - SCT2A25:**
Replaces the unstable 12V XL7005A with a robust 100V-rated step-down (SCT2A25).
- Output is tuned to **10V** instead of 12V. 
- **Feedback Divider**: $V_{FB} = 1.2V$
  - $V_{OUT} = 1.2 \times (1 + R_{up}/R_{down})$
  - $R_{up} = 73.2k\Omega$, $R_{down} = 10k\Omega \implies V_{OUT} = 9.984V \approx 10V$
- **Switching Frequency**: Fixed at 300kHz.
- **Inductor ($L_1$)**: For 10V out, a standard **33µH or 47µH** inductor (e.g., 6x6mm shielded) is suitable.
- **Input Capacitor ($C_{IN}$)**: Minimum **2x 2.2µF 100V X7R** MLCCs + **0.1µF** high-frequency bypass.
- **Output Capacitor ($C_{OUT}$)**: Minimum **2x 22µF 25V X7R/X5R** MLCCs.
- **Catch Diode**: **SS510** (100V, 5A Schottky) or equivalent.
- **Bootstrap Capacitor**: **0.1µF 50V** ceramic.
- **UVLO Divider**: To set start at ~15V and stop at ~14V: $R_{UVLO\_TOP} = 464k\Omega$, $R_{UVLO\_BOT} = 42.2k\Omega$.

**Secondary Aux Supply (3.3V Logic) - SY8120:**
- Uses a "Cascaded Buck" architecture: 10V $\rightarrow$ Tiny Sync Buck (SY8120) $\rightarrow$ 3.3V.
- **Feedback Divider**: $V_{FB} = 0.6V$
  - $V_{OUT} = 0.6 \times (1 + R_{up}/R_{down})$
  - $R_{up} = 100k\Omega$, $R_{down} = 22.1k\Omega \implies V_{OUT} = 0.6 \times (1 + 100/22.1) = 3.31V \approx 3.3V$
- **Switching Frequency**: Fixed at 500kHz.
- **Inductor ($L$)**: **4.7µH** (recommended for 3.3V out).
- **Input Capacitor ($C_{IN}$)**: **10µF 16V** ceramic.
- **Output Capacitor ($C_{OUT}$)**: **10µF 10V** ceramic (22µF is also acceptable).
- **Feedforward Capacitor ($C_{FF}$)**: **47pF** ceramic.
- **Bootstrap Capacitor ($C_{BS}$)**: **0.1µF** ceramic.

## 3. Inductor Sizing (Main Power Stage)
Targeting a peak-to-peak ripple current ($\Delta I_L$) of 20% of $I_{out,max}$ (**4.0 A**).
Because this is a 4-switch Buck-Boost, the inductor must satisfy both Buck and Boost continuous conduction modes (CCM).

**Buck Mode Worst-Case:** (50% Duty Cycle, e.g., 80V in, 40V out)
$L_{buck} = \frac{V_{out} \times (V_{in} - V_{out})}{\Delta I_L \times f_{sw} \times V_{in}}$
$L_{buck} = \frac{40 \times (80 - 40)}{4 \times 100,000 \times 80} = \mathbf{50.0 \mu H}$

**Boost Mode Worst-Case:** (High Step-Up, e.g., 12V in, 58V out)
$L_{boost} = \frac{V_{in} \times (V_{out} - V_{in})}{\Delta I_L \times f_{sw} \times V_{out}}$
$L_{boost} = \frac{12 \times (58 - 12)}{4 \times 100,000 \times 58} \approx \mathbf{23.8 \mu H}$

**v1.3 Component Selection:**
To satisfy both extremes while minimizing heat, the target inductor size should be between **33 µH and 47 µH**.
- *Note:* The v1.1 implementation used 100µH, which resulted in very low ripple but high copper losses (DCR). Lowering to ~47µH will improve transient response and reduce inductor heating, provided the capacitors can handle the slightly higher ripple.

## 4. Bulk Capacitor Selection
Targeting an output voltage ripple ($\Delta V_{out}$) of **< 100mV**.

**Ideal Capacitance (Assuming Zero ESR):**
$C_{out,min} = \frac{\Delta I_L}{8 \times f_{sw} \times \Delta V_{out}}$
$C_{out,min} = \frac{4.0}{8 \times 100,000 \times 0.1} = \mathbf{50.0 \mu F}$

**Real-World Constraints (ESR Dominates):**
At 100kHz, the voltage ripple is almost entirely dictated by the Equivalent Series Resistance (ESR) of the capacitors, not the pure capacitance value:
$ESR_{max} = \frac{\Delta V_{out}}{\Delta I_L} = \frac{0.1V}{4.0A} = \mathbf{25 m\Omega}$

**v1.3 Capacitor Strategy:**
To achieve an ESR < 25 mΩ and handle the heavy ripple current without overheating, we must use a parallel bank of low-ESR capacitors.
- *Recommendation*: Use **4x 100µF to 220µF** Aluminum Polymer or high-grade Electrolytic capacitors in parallel. If using standard electrolytics, they typically have ~80mΩ ESR each, so 4 in parallel gives ~20mΩ (passing our requirement). Ceramic MLCCs (e.g., 4x 10µF 100V X7R) should also be added directly at the MOSFET switching nodes to handle the high-frequency edge spikes.

## 4. Voltage Divider & ADC Scaling
The ADC measures 0-3.3V. We scale $V_{in}$ and $V_{out}$ to fit this range.

**Current v1.1 Setup (e.g., VIN):**
- $R_{top} = 200 k\Omega$ (`R2`)
- $R_{bottom} = 10 k\Omega$ (`R9`)
- Ratio: $10 / (200 + 10) = 0.0476$
- $V_{max\_measurable} = 3.3V / 0.0476 = 69.3V$

*Note: For v1.3 with 80V support, $R_{top}$ or $R_{bottom}$ must be adjusted to increase the measurable range.*

## 5. Current Sense (INA240A4)
- Gain: 200 V/V
- Shunt Resistor: $1 m\Omega$ (`R18-R25` parallel network)
- $V_{out} = I_{load} \times R_{shunt} \times Gain$
- At 20A: $20A \times 0.001 \Omega \times 200 = 4.0V$
- *Correction required for v1.3*: 4.0V exceeds the 3.3V MCU rail. Shunt value or Gain must be reduced.

## 6. Thermal & Trace Width
Using IPC-2221 standards for 10°C rise at 20A:
- Required Cross-section: ~300 mils²
- At 2oz copper (70µm): Required width $\approx$ 10.5mm
- *Current v1.1 Strategy*: 3.5mm traces on Top + Bottom layers + exposed copper with solder reinforcement.

## 7. Analog Low-Pass Filters (Anti-Aliasing)
RC filters are used on ADC inputs to suppress 100kHz switching noise and prevent aliasing.
Cutoff frequency: $f_c = \frac{1}{2 \pi R C}$

**Voltage Sensing LPF (v1.1):**
- $R_{source} \approx R_{bottom} = 10 k\Omega$ (Parallel with $R_{top}$ is dominated by $R_{bottom}$)
- $C_{filter} = 100 nF$ (`C12`, `C13`)
- $f_c = \frac{1}{2 \pi \times 10,000 \times 100 \times 10^{-9}} \approx 159 Hz$
- *Evaluation*: Excellent noise rejection, but introduces significant phase lag (~1ms). Acceptable for battery voltage monitoring.

**Current Sensing LPF (v1.1):**
- INA240 output has low impedance. Series $R = 1.5 k\Omega$ (`R20`).
- $C_{filter} = 10 nF$ (`C34`).
- $f_c = \frac{1}{2 \pi \times 1500 \times 10 \times 10^{-9}} \approx 10.6 kHz$
- *Evaluation*: Good balance for 1.5kHz control loop. Provides ~20dB attenuation at 100kHz.

**v1.3 Proposed Optimization:**
- To support faster transient response in E-Bike mode, consider reducing $C_{filter}$ for voltage sensing to $10nF$ ($f_c \approx 1.6kHz$) if the software EMA filter is sufficiently aggressive.
