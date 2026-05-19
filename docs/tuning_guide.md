# MPPT Auto-Tuning: Hardware Test Procedure

## Current Status
The **Hybrid Auto-Tuning** system is fully implemented. The firmware supports dynamic `CMD:TUNE_*` commands, and the Python controller `scripts/tune_mppt.py` is ready for hardware-in-the-loop (HIL) testing.

## Lab Setup (Simulated Solar Panel)
To simulate a solar panel curve using a standard 60V/5A Lab PSU:
1.  **Source Side**: Connect the PSU in series with a **4.7 Ω (100W+)** resistor.
2.  **Load Side**: Connect the MPPT output to a **2.2 Ω (200W+)** resistor (simulates a 12V battery under charge).
3.  **Cooling**: Ensure resistors have active cooling, as they will dissipate ~100W during 45s test windows.

## Running the Tuning Session
1.  **Environment**: Ensure `pyserial` is installed (`pip install pyserial`).
2.  **Execution**:
    ```bash
    python scripts/tune_mppt.py --port /dev/ttyACM0 --random-iters 15 --fine-iters 10
    ```
3.  **Algorithm**:
    *   **Phase 1 (Random Search)**: Explores a wide range of $N$-Factors, step sizes, and EMA filtering.
    *   **Phase 2 (Fine Tuning)**: Squeezes maximum power by optimizing around the best local candidate.
4.  **Monitoring**:
    *   Check `tuning_logs/summary_*.csv` for a ranking of all tested configurations.
    *   Individual run telemetry is saved to separate CSVs for post-mortem analysis of stability vs. efficiency.

## Next Steps for AI Agent
1.  Verify the serial connection to the device.
2.  Assist the user in executing the tuning script.
3.  Analyze the `summary_*.csv` file to identify the most stable configuration.
4.  Optionally, persist the "Optimal Parameters" to the firmware's `system_config.h` or via `CMD:LIMITS_SAVE` if permanent storage is required.
