# MPPT Tuning & Architecture Guide

## Control Architecture: The Unified Min-Selector
The system has moved away from discrete state-based PID loops. It now uses a **Unified Override Control** model:
1.  **Multiple Deltas**: Every control objective (MPPT, Output CV, Output CC, Input UV, etc.) calculates its own desired change ($\Delta$) in duty cycle.
2.  **Min-Selector**: The controller selects the **minimum** $\Delta$ among all active constraints.
    *   Example: If MPPT wants to increase duty (+10) but Output CV needs to decrease it to stay at 14.4V (-5), the controller picks -5.
3.  **Velocity PI**: The selected $\Delta$ is accumulated into a 64-bit high-resolution integrator, which is then applied to the hardware PWM.

## Telemetry States
The `state` string in the telemetry now provides granular feedback on which limit is currently "winning" the selector:
- `ACTIVE_TRACKING`: No limits hit, MPPT is in control.
- `ACTIVE_CV`: Output Voltage limit is active (Battery Full).
- `ACTIVE_CC`: Output Current limit is active.
- `ACTIVE_BROWNOUT`: Input Voltage is dropping below the soft-floor.
- `ACTIVE_VIN_LIMIT`: Input Voltage is hitting the upper limit (Regen safety).
- `ACTIVE_REVERSE`: Reverse current/regen limit is active.

## Control Gains
- `GAIN_KP`: Proportional gain. Currently tuned to **5** for stable transitions on openMPPT v1.1 hardware.
- `GAIN_KI`: Integral gain. Currently **2**.
- `SOFT_LIMIT_HOLD_TIME_MS`: Set to **100ms** to prevent dashboard flickering when bouncing on a limit.

## Lab Setup (Simulated Solar Panel)
To simulate a solar panel curve using a standard 60V/5A Lab PSU:
1.  **Source Side**: Connect the PSU in series with a **4.7 Ω (100W+)** resistor.
2.  **Load Side**: Connect the MPPT output to a **2.2 Ω (200W+)** resistor or a 12V battery.
3.  **Cooling**: Ensure resistors have active cooling.

## Running the Tuning Session
1.  **Environment**: Ensure `pyserial` is installed (`pip install pyserial`).
2.  **Execution**:
    ```bash
    python scripts/tune_mppt.py --port /dev/ttyACM0 --random-iters 15 --fine-iters 10
    ```
3.  **Algorithm**:
    *   **Phase 1 (Random Search)**: Explores step sizes and EMA filtering.
    *   **Phase 2 (Fine Tuning)**: Optimizes around the best local candidate.
