import serial
import json
import time
import sys
import os
import threading
import argparse
from datetime import datetime

class MPPTTool:
    def __init__(self, port, baud=115200, log_file='mppt_log.csv'):
        self.port = port
        self.baud = baud
        self.log_file = log_file
        self.ser = None
        self.running = False
        self.telemetry_history = []
        self.max_history = 500  # Keep last 500 points (~50s at 10Hz)
        self.lock = threading.Lock()

    def connect(self):
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.1)
            print(f"Connected to {self.port}")
            return True
        except Exception as e:
            print(f"Connection failed: {e}")
            return False

    def send_cmd(self, cmd):
        if not self.ser:
            return "Not connected"
        
        full_cmd = f"{cmd}\n"
        with self.lock:
            self.ser.write(full_cmd.encode())
            print(f"Sent: {cmd}")
            
        # Wait for ACK or response
        start = time.time()
        while time.time() - start < 1.0:
            line = self.ser.readline().decode('utf-8', errors='ignore').strip()
            if line.startswith("ACK:") or line.startswith("{"):
                return line
        return "Timeout"

    def _log_worker(self):
        with open(self.log_file, 'a') as f:
            if os.stat(self.log_file).st_size == 0:
                f.write("timestamp,Vin_mV,Vout_mV,Ain_mA,Aout_mA,Win_mW,Wout_mW,duty_x100,mppt_step,state,fault\n")
            
            while self.running:
                try:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    if not line:
                        continue
                    
                    if line.startswith('{"type":"telemetry"'):
                        data = json.loads(line)
                        ts = datetime.now().isoformat()
                        data['ts'] = ts
                        
                        csv_line = f"{ts},{data['Vin_mV']},{data['Vout_mV']},{data['Ain_mA']},{data['Aout_mA']}," \
                                   f"{data['Win_mW']},{data['Wout_mW']},{data['duty_x100']},{data['mppt_step']}," \
                                   f"{data['state']},{data['fault_reason']}\n"
                        
                        with self.lock:
                            f.write(csv_line)
                            f.flush()
                            self.telemetry_history.append(data)
                            if len(self.telemetry_history) > self.max_history:
                                self.telemetry_history.pop(0)
                        
                        if data['fault_reason'] != 'NONE':
                            print(f"\n[!] FAULT DETECTED: {data['fault_reason']} at {ts}")
                            if self.watch_mode:
                                self.save_crash_dump(data['fault_reason'])
                                self.running = False
                            
                    elif line.startswith("STATE:"):
                        print(f"\n[*] {line}")
                    elif line.startswith("ACK:"):
                        print(f"\n[+] {line}")
                        
                except Exception as e:
                    pass

    def save_crash_dump(self, reason):
        dump_file = f"fault_dump_{int(time.time())}.json"
        with open(dump_file, 'w') as f:
            json.dump({
                "reason": reason,
                "history": self.telemetry_history
            }, f, indent=2)
        print(f"Crash dump saved to {dump_file}")

    def start_logging(self, watch=False):
        self.watch_mode = watch
        self.running = True
        self.thread = threading.Thread(target=self._log_worker, daemon=True)
        self.thread.start()

    def stop(self):
        self.running = False
        if self.ser:
            self.ser.close()

def main():
    parser = argparse.ArgumentParser(description="MPPT Debug & Control Tool")
    parser.add_argument("--port", default="/dev/ttyACM0", help="Serial port")
    parser.add_argument("--cmd", help="Send a single command and exit")
    parser.add_argument("--monitor", action="store_true", help="Monitor and log telemetry")
    parser.add_argument("--watch", action="store_true", help="Watch for fault, save dump, and exit")
    parser.add_argument("--reset", action="store_true", help="Reset fault and exit")
    
    args = parser.parse_args()

    # Auto-detect port if default fails
    if not os.path.exists(args.port):
        for i in range(10):
            p = f"/dev/ttyACM{i}"
            if os.path.exists(p):
                args.port = p
                break

    tool = MPPTTool(args.port)
    if not tool.connect():
        sys.exit(1)

    if args.reset:
        print(tool.send_cmd("CMD:RESET_FAULT"))
        sys.exit(0)

    if args.cmd:
        print(tool.send_cmd(args.cmd))
        sys.exit(0)

    if args.monitor or args.watch:
        tool.start_logging(watch=args.watch)
        if args.watch:
            print("Watching for fault... (Press Ctrl+C to stop)")
            try:
                while tool.running:
                    time.sleep(0.1)
            except KeyboardInterrupt:
                pass
            finally:
                tool.stop()
            sys.exit(0)

        print("Monitoring... Press Enter to send commands, Ctrl+C to stop.")
        try:
            while True:
                cmd = input("> ")
                if cmd.lower() in ['exit', 'quit']:
                    break
                if cmd:
                    if not cmd.startswith("CMD:"):
                        cmd = f"CMD:{cmd.upper()}"
                    resp = tool.send_cmd(cmd)
                    print(f"Response: {resp}")
        except KeyboardInterrupt:
            pass
        finally:
            tool.stop()

if __name__ == "__main__":
    main()
