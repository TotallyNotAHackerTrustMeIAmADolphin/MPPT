import serial
import json
import time
import random
import csv
import argparse
import os
import statistics
from datetime import datetime

class MpptTuner:
    def __init__(self, port, baud=115200, log_dir="tuning_logs"):
        self.ser = serial.Serial(port, baud, timeout=0.1)
        self.log_dir = log_dir
        if not os.path.exists(log_dir):
            os.makedirs(log_dir)
        
        self.session_id = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.summary_file = os.path.join(self.log_dir, f"summary_fixed_step_{self.session_id}.csv")
        
        # Initialize summary CSV
        with open(self.summary_file, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(["timestamp", "step_size", "threshold", "ema", "interval_derived", 
                             "avg_power_mW", "std_power_mW", "std_vin_mV", "std_iin_mA", 
                             "time_to_mpp_s", "score"])

    def send_command(self, cmd):
        self.ser.write(f"{cmd}\n".encode())
        time.sleep(0.05)
        # Clear buffer
        while self.ser.in_waiting:
            self.ser.readline()

    def apply_params(self, params):
        # Mathematically link cycle time to EMA smoothing
        # Assuming high-rate task is 200Hz (5ms per sample)
        # 3 time constants (95% settling) = 3 * (2^ema) samples
        # interval = samples * 5ms
        interval = int(3 * (2 ** params['ema']) * 5)
        # Clamp interval to reasonable bounds to prevent watchdog resets or extreme sluggishness
        interval = max(20, min(1000, interval))
        params['interval_derived'] = interval

        print(f"Applying: step={params['step_size']}, thresh={params['threshold']}, ema={params['ema']} (derived_int={interval}ms)")
        self.send_command("CMD:TUNE_RESET")
        time.sleep(0.5)
        # N-Factor and Max Step are ignored by simplified firmware, but we send dummy values
        self.send_command("CMD:TUNE_N:0")
        self.send_command("CMD:TUNE_MAX:2000") 
        
        self.send_command(f"CMD:TUNE_MIN:{params['step_size']}")
        self.send_command(f"CMD:TUNE_THRESH:{params['threshold']}")
        self.send_command(f"CMD:TUNE_INT:{interval}")
        self.send_command(f"CMD:TUNE_EMA:{params['ema']}")

    def evaluate(self, params, total_time=45):
        # 1. Apply params (this also resets tracking via TUNE_RESET)
        self.apply_params(params)
        
        # 2. Simulate Cold Start (Force into CV mode)
        print("Simulating cold start (V_MAX = 1V)...")
        self.send_command("CMD:SET_V_MAX:1000")
        time.sleep(2.0) # Wait for duty cycle to collapse
        
        # 3. Restore V_MAX and start evaluation
        print(f"Restoring V_MAX and recording for {total_time}s...")
        self.send_command("CMD:SET_V_MAX:80000")
        
        frames = []
        start_time = time.time()
        
        run_id = f"run_fixed_S{params['step_size']}_T{params['threshold']}_E{params['ema']}_{int(start_time)}"
        run_file = os.path.join(self.log_dir, f"{run_id}.csv")
        
        fieldnames = ["time_offset", "type", "Vin_mV", "Vout_mV", "Ain_mA", "Aout_mA", "Win_mW", "Wout_mW", "duty_x100", "mppt_step", "eff", "temp_C", "state", "fault_reason"]
        
        with open(run_file, 'w', newline='') as f:
            writer = None
            
            while time.time() - start_time < total_time:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if line.startswith('{'):
                    try:
                        data = json.loads(line)
                        if data.get("type") == "telemetry":
                            data["time_offset"] = round(time.time() - start_time, 2)
                            frames.append(data)
                            if writer is None:
                                writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction='ignore')
                                writer.writeheader()
                            writer.writerow(data)
                    except json.JSONDecodeError:
                        continue
        
        if len(frames) < 50:
            print("Error: No telemetry received!")
            return 0
        
        # Split into "climb" and "steady state"
        # Assume the last 15 seconds represent the steady state
        steady_state_frames = [f for f in frames if f["time_offset"] > (total_time - 15)]
        
        if not steady_state_frames:
            print("Error: No frames in steady state window!")
            return 0

        ss_powers = [f["Wout_mW"] for f in steady_state_frames]
        ss_vins = [f["Vin_mV"] for f in steady_state_frames]
        ss_iins = [f["Ain_mA"] for f in steady_state_frames]
        ss_effs = [f["eff"] for f in steady_state_frames]
        
        avg_power = statistics.mean(ss_powers)
        std_power = statistics.stdev(ss_powers) if len(ss_powers) > 1 else 0
        std_vin = statistics.stdev(ss_vins) if len(ss_vins) > 1 else 0
        std_iin = statistics.stdev(ss_iins) if len(ss_iins) > 1 else 0
        avg_eff = statistics.mean(ss_effs)
        
        # Calculate time to MPP (time to reach 95% of steady-state avg_power)
        target_power = avg_power * 0.95
        time_to_mpp = total_time # default to max
        for f in frames:
            if f["Wout_mW"] >= target_power:
                time_to_mpp = f["time_offset"]
                break
                
        # 1. Stability Penalty (Power Jitter)
        variation = std_power / avg_power if avg_power > 0 else 1.0
        stability_multiplier = 1.0
        if variation > 0.02:
            if variation < 0.10:
                stability_multiplier = 1.0 - (variation - 0.02) * 5.0
            else:
                stability_multiplier = 0.6 * (0.10 / variation) ** 2
                
        # 2. Input Jitter Penalty (Vin and Iin)
        # punish configs that cause >5% swings on input
        vin_variation = std_vin / statistics.mean(ss_vins) if statistics.mean(ss_vins) > 0 else 1.0
        iin_variation = std_iin / statistics.mean(ss_iins) if statistics.mean(ss_iins) > 0 else 1.0
        
        input_jitter_penalty = 1.0
        if vin_variation > 0.05 or iin_variation > 0.05:
            input_jitter_penalty = 0.8
        if vin_variation > 0.10 or iin_variation > 0.10:
            input_jitter_penalty = 0.5
            
        # 3. Time-to-MPP Penalty
        # If it takes > 20s to find the peak, penalize
        speed_multiplier = 1.0
        if time_to_mpp > 20:
            speed_multiplier = max(0.5, 1.0 - ((time_to_mpp - 20) * 0.02))
            
        # Efficiency Multiplier
        eff_multiplier = avg_eff / 100.0 if avg_eff > 0 else 0.0
        
        # Final Score
        score = avg_power * stability_multiplier * input_jitter_penalty * speed_multiplier * eff_multiplier
        
        print(f"Result: Pwr={avg_power:.1f}mW, PwrStd={std_power:.1f}mW ({variation*100:.1f}%)")
        print(f"        VinStd={std_vin:.1f}mV, IinStd={std_iin:.1f}mA")
        print(f"        TimeToMPP={time_to_mpp:.2f}s, Eff={avg_eff:.1f}%, Score={score:.2f}")
        
        # Save to summary
        with open(self.summary_file, 'a', newline='') as f:
            writer = csv.writer(f)
            writer.writerow([datetime.now().isoformat(), params['step_size'], params['threshold'], params['ema'], params['interval_derived'], 
                             avg_power, std_power, std_vin, std_iin, time_to_mpp, score])
            
        return score

def random_search(tuner, iterations=15):
    best_score = -1
    best_params = None
    
    print(f"Starting Advanced Random Search ({iterations} iterations)...")
    
    for i in range(iterations):
        print(f"\nIteration {i+1}/{iterations}")
        params = {
            'step_size': random.randint(1, 40),
            'threshold': random.randint(20000, 300000),
            'ema': random.randint(1, 6)
        }
            
        score = tuner.evaluate(params)
        if score > best_score:
            best_score = score
            best_params = params
            print(f"*** New Best Score: {best_score:.2f} ***")
            
    return best_params, best_score

def local_fine_tune(tuner, initial_params, iterations=10):
    print(f"\nStarting Fine-Tuning around: {initial_params}")
    best_params = initial_params.copy()
    best_score = tuner.evaluate(best_params)
    
    for i in range(iterations):
        print(f"\nFine-Tune Iteration {i+1}/{iterations}")
        params = {}
        for k, v in best_params.items():
            if k == 'ema':
                params[k] = max(1, min(7, v + random.randint(-1, 1)))
            else:
                change = int(v * random.uniform(-0.15, 0.15))
                if change == 0: change = random.choice([-1, 1])
                params[k] = v + change
                
        # Clamp to ranges
        params['step_size'] = max(1, min(80, params['step_size']))
        params['threshold'] = max(10000, min(500000, params['threshold']))
            
        score = tuner.evaluate(params)
        if score > best_score:
            best_score = score
            best_params = params
            print(f"*** New Best Score in Fine-Tune: {best_score:.2f} ***")
            
    return best_params, best_score

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Advanced Fixed-Step MPPT Auto-Tuner')
    parser.add_argument('--port', type=str, required=True, help='Serial port')
    parser.add_argument('--random-iters', type=int, default=10, help='Number of random search iterations')
    parser.add_argument('--fine-iters', type=int, default=5, help='Number of fine-tuning iterations')
    
    args = parser.parse_args()
    
    tuner = MpptTuner(args.port)
    
    try:
        # Phase 1: Random Search
        best_p, best_s = random_search(tuner, iterations=args.random_iters)
        
        # Phase 2: Fine Tuning
        final_p, final_s = local_fine_tune(tuner, best_p, iterations=args.fine_iters)
        
        print("\n" + "="*40)
        print("ADVANCED TUNING COMPLETE")
        print(f"Final Score: {final_s:.2f}")
        print("Optimal Parameters:")
        for k, v in final_p.items():
            if k == 'interval_derived': continue
            print(f"  {k}: {v}")
        print("="*40)
        
        # Apply final params
        tuner.apply_params(final_p)
        print("Final parameters applied to device.")
        
    except KeyboardInterrupt:
        print("\nTuning interrupted by user.")
    except Exception as e:
        print(f"\nAn error occurred: {e}")
    finally:
        tuner.ser.close()
