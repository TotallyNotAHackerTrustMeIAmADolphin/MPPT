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
**Primary Aux Supply (10V):**
Replaces the unstable 12V XL7005A with a robust 100V-rated step-down (e.g., SCT2A25).
- Output is tuned to **10V** instead of 12V. 
- *Why 10V?* The CSD19505 is fully enhanced at $V_{GS}=10V$ ($R_{DS(on)}$ = 11 mΩ). Driving at 10V instead of 12V reduces the gate charge power dissipation ($P_{gate} = Q_g \times V_{drive} \times f_{sw}$) by ~16%, shifting thermal load away from the sensitive IRS21867 gate drivers onto the heatsinked TO-220 MOSFETs.

**Secondary Aux Supply (3.3V Logic):**
- Uses a "Cascaded Buck" architecture: 10V $\rightarrow$ Tiny Sync Buck $\rightarrow$ 3.3V.
- *Why?* Prevents the catastrophic "Load Dump Cascade" where an 80V spike kills the primary regulator and shorts directly into the 3.3V MCU rail.

## 3. Inductor Sizing
Targeting a ripple current ($\Delta I_L$) of 20-40% of $I_{out,max}$.
For a Buck converter: $L = \frac{V_{out} \times (V_{in} - V_{out})}{\Delta I_L \times f_{sw} \times V_{in}}$

**Calculations for 48V Battery (58V Charge) from 80V Panel:**
- $V_{in} = 80V$, $V_{out} = 58V$
- Target $\Delta I_L = 4A$ (20% of 20A)
- $L = \frac{58 \times (80 - 58)}{4 \times 100,000 \times 80} \approx 40 \mu H$

*Current v1.1 Implementation uses 100µH (L2, L3 in parallel or series configuration depending on assembly), which provides significantly lower ripple but higher ESR.*

## 3. Output Capacitor Selection
Targeting output voltage ripple ($\Delta V_{out}$) < 100mV.
$\Delta V_{out} = \frac{\Delta I_L}{8 \times f_{sw} \times C_{out}}$

**For $\Delta I_L = 4A$ and $C_{out} = 440 \mu F$ (4x 110µF):**
- $\Delta V_{out} = \frac{4}{8 \times 100,000 \times 440 \times 10^{-6}} \approx 11 mV$

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
