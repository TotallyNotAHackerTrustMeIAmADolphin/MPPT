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
  - Using E96 series: $R_{up} = 110k\Omega$, $R_{down} = 15k\Omega \implies V_{OUT} = 1.2 \times (1 + 110/15) = 10.0V$
- **Switching Frequency**: Fixed at 300kHz.
- **Inductor ($L_1$)**: For 10V out, a standard **33µH or 47µH** inductor (e.g., 6x6mm shielded) is suitable.
- **UVLO Divider**: To set start at ~12.5V: $R_{UVLO\_TOP} = 430k\Omega$, $R_{UVLO\_BOT} = 47k\Omega$ (Both are E24).
- **Bootstrap Diode**: **US1M** (1000V, 1A Ultrafast, $t_{rr} \le 75ns$).

**Secondary Aux Supply (3.3V Logic) - SY8120:**
- Uses a "Cascaded Buck" architecture: 10V $\rightarrow$ Tiny Sync Buck (SY8120) $\rightarrow$ 3.3V.
- **Feedback Divider**: $V_{FB} = 0.6V$
  - $V_{OUT} = 0.6 \times (1 + R_{up}/R_{down})$
  - Using E24 series: $R_{up} = 68k\Omega$, $R_{down} = 15k\Omega \implies V_{OUT} = 0.6 \times (1 + 68/15) = 3.32V \approx 3.3V$
- **Switching Frequency**: Fixed at 500kHz.
- **Inductor ($L$)**: **4.7µH** (recommended for 3.3V out).

## 3. Transient Voltage Suppression (TVS) Strategy
**Primary Bus Protection (80V):**
- **Component**: **5.0SMDJ85CA** (Bidirectional, 5kW).
- **Rationale**: Standoff ($V_{RWM}$) of 85V ensures no leakage at 80V. 5kW rating handles massive inductive kickback and solar surges.
- **Placement**: Immediately adjacent to VIN/VOUT XT60 connectors.

**Gate Drive Protection (10V):**
- **Component**: **H12VH22U** (6kW Surge).
- **Rationale**: 12V standoff ensures invisibility at 10V rail. Protects sensitive IRS21867 drivers (25V max) from regulator failure.

**Logic Protection (3.3V):**
- **Component**: **H3V3L06B** (ESD).
- **Rationale**: 3.3V standoff is required because STM32 Absolute Max is only 4.0V. Provides last line of defense against ESD and noise.

## 4. Inductor Sizing (Main Power Stage)
Targeting a peak-to-peak ripple current ($\Delta I_L$) of 20% of $I_{out,max}$ (**4.0 A**).

**Buck Mode Worst-Case:** (50% Duty Cycle, e.g., 80V in, 40V out)
$L_{buck} = \frac{V_{out} \times (V_{in} - V_{out})}{\Delta I_L \times f_{sw} \times V_{in}} = \mathbf{50.0 \mu H}$

**v1.3 Component Selection:**
Target inductor size: **33 µH to 47 µH**.
- **CRITICAL**: Saturation Current ($I_{sat}$) **MUST be > 25A**. Using a lower $I_{sat}$ (like 11A) will cause magnetic collapse and MOSFET destruction.

## 5. Bulk Capacitor Selection
Targeting an output voltage ripple ($\Delta V_{out}$) of **< 100mV**.
$ESR_{max} = \frac{\Delta V_{out}}{\Delta I_L} = \frac{0.1V}{4.0A} = \mathbf{25 m\Omega}$

**v1.3 Capacitor Strategy:**
- Use **4x 330µF 100V Low-ESR Electrolytic** (e.g., Ymin LKML series) in parallel.
- **Total Bank ESR**: $\approx 11.75 m\Omega$ (Passes 25mΩ limit).
- **Total Ripple Capacity**: $\approx 8.5 A$ (Safe for 20A operation).

## 6. Voltage Divider & ADC Scaling
**Voltage Sensing LPF (v1.3 Optimized):**
- $R_{top} = 200 k\Omega$, $R_{bottom} = 4.7 k\Omega$.
- $R_{th} \approx 4.59 k\Omega$
- $C_{filter} = 10 nF$
- $f_c = \frac{1}{2 \pi \times 4,590 \times 10 \times 10^{-9}} \approx 3.47 kHz$
- *Evaluation*: Balanced for high-speed tracking and noise rejection.

## 7. Current Sense (CC6937)
- **Type**: Isolated Hall Effect.
- **Sensitivity**: Check variant (e.g., 66mV/A for 20A).
- **Range**: +/- 20A (Full scale $\approx 2.97V$ on 3.3V ADC).
- **VREF**: 100nF bypass to GND required.
