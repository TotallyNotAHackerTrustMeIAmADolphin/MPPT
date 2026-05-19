import serial
import json
import time
import csv
import argparse
import os
import statistics
from datetime import datetime
import random

class MpptTuner:
    def __init__(self, port, baud=115200, log_dir="tuning_logs"):
        self.ser = serial.Serial(port, baud, timeout=0.1)
        self.log_dir = log_dir
        if not os.path.exists(log_dir):
            os.makedirs(log_dir)
        
        self.session_id = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.summary_file = os.path.join(self.log_dir, f"summary_resume_{self.session_id}.csv")
        
        with open(self.summary_file, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(["timestamp", "n_factor", "min_step", "max_step", "threshold", "interval", "ema", "avg_power_mW", "std_power_mW", "score"])

    def send_command(self, cmd):
        self.ser.write(f"{cmd}\n".encode())
        time.sleep(0.05)
        while self.ser.in_waiting:
            self.ser.readline()

    def apply_params(self, params):
        print(f"Applying: {params}")
        self.send_command("CMD:TUNE_RESET")
        time.sleep(0.5)
        self.send_command(f"CMD:TUNE_N:{params['n_factor']}")
        self.send_command(f"CMD:TUNE_MIN:{params['min_step']}")
        self.send_command(f"CMD:TUNE_MAX:{params['max_step']}")
        self.send_command(f"CMD:TUNE_THRESH:{params['threshold']}")
        self.send_command(f"CMD:TUNE_INT:{params['interval']}")
        self.send_command(f"CMD:TUNE_EMA:{params['ema']}")

    def evaluate(self, params, settle_time=20, eval_time=25):
        self.apply_params(params)
        print(f"Settling for {settle_time}s...")
        time.sleep(settle_time)
        print(f"Evaluating for {eval_time}s...")
        frames = []
        start_time = time.time()
        
        run_id = f"resume_run_N{params['n_factor']}_min{params['min_step']}_max{params['max_step']}_{int(start_time)}"
        run_file = os.path.join(self.log_dir, f"{run_id}.csv")
        
        fieldnames = ["type", "Vin_mV", "Vout_mV", "Ain_mA", "Aout_mA", "Win_mW", "Wout_mW", "duty_x100", "mppt_step", "eff", "temp_C", "state", "fault_reason"]
        
        with open(run_file, 'w', newline='') as f:
            writer = None
            while time.time() - start_time < eval_time:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if line.startswith('{'):
                    try:
                        data = json.loads(line)
                        if data.get("type") == "telemetry":
                            frames.append(data)
                            if writer is None:
                                writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction='ignore')
                                writer.writeheader()
                            writer.writerow(data)
                    except json.JSONDecodeError:
                        continue
        
        if not frames:
            print("Error: No telemetry received!")
            return 0
        
        powers = [f["Wout_mW"] for f in frames]
        efficiencies = [f["eff"] for f in frames]
        avg_power = statistics.mean(powers)
        std_power = statistics.stdev(powers) if len(powers) > 1 else 0
        avg_eff = statistics.mean(efficiencies)
        
        variation = std_power / avg_power if avg_power > 0 else 1.0
        stability_multiplier = 1.0
        if variation > 0.02:
            if variation < 0.10:
                stability_multiplier = 1.0 - (variation - 0.02) * 5.0
            else:
                stability_multiplier = 0.6 * (0.10 / variation) ** 2
        
        eff_multiplier = avg_eff / 100.0 if avg_eff > 0 else 0.0
        score = avg_power * stability_multiplier * eff_multiplier
        
        print(f"Result: Power={avg_power:.1f}mW, Std={std_power:.1f}mW ({variation*100:.1f}%), Eff={avg_eff:.1f}%, Score={score:.2f}")
        with open(self.summary_file, 'a', newline='') as f:
            writer = csv.writer(f)
            writer.writerow([datetime.now().isoformat(), params['n_factor'], params['min_step'], params['max_step'], params['threshold'], params['interval'], params['ema'], avg_power, std_power, score])
        return score

def local_fine_tune(tuner, initial_params, iterations=10):
    print(f"\nResuming Fine-Tuning around: {initial_params}")
    best_params = initial_params.copy()
    best_score = tuner.evaluate(best_params)
    
    for i in range(iterations):
        print(f"\nFine-Tune Iteration {i+1}/{iterations}")
        params = {}
        for k, v in best_params.items():
            if k == 'ema':
                params[k] = max(0, min(8, v + random.randint(-1, 1)))
            else:
                change = int(v * random.uniform(-0.15, 0.15))
                if change == 0: change = random.choice([-1, 1])
                params[k] = v + change
        
        params['n_factor'] = max(10, min(200, params['n_factor']))
        params['min_step'] = max(1, min(20, params['min_step']))
        params['max_step'] = max(20, min(100, params['max_step']))
        params['threshold'] = max(50000, min(500000, params['threshold']))
        params['interval'] = max(10, min(200, params['interval']))
        
        if params['max_step'] <= params['min_step']:
            params['max_step'] = params['min_step'] + 5
            
        score = tuner.evaluate(params)
        if score > best_score:
            best_score = score
            best_params = params
            print(f"*** New Best Score in Fine-Tune: {best_score:.2f} ***")
    return best_params, best_score

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=str, required=True)
    args = parser.parse_args()
    
    tuner = MpptTuner(args.port)
    
    # Manually seeded best parameters from previous failed run
    best_p = {
        'n_factor': 31, 
        'min_step': 1, 
        'max_step': 39, 
        'threshold': 118431, 
        'interval': 103, 
        'ema': 7
    }
    
    try:
        final_p, final_s = local_fine_tune(tuner, best_p, iterations=5)
        print("\n" + "="*40)
        print("RESUMED TUNING COMPLETE")
        print(f"Final Score: {final_s:.2f}")
        print("Optimal Parameters:")
        for k, v in final_p.items():
            print(f"  {k}: {v}")
        print("="*40)
        tuner.apply_params(final_p)
    except Exception as e:
        print(f"Error: {e}")
    finally:
        tuner.ser.close()
