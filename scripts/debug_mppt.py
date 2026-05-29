import serial
import json
import time
import sys
import os

# Configuration
PORT = '/dev/ttyACM0'  # Default, will try to detect or use environment
BAUD = 115200
LOG_FILE = 'mppt_debug_log.csv'

def find_serial_port():
    if os.path.exists(PORT):
        return PORT
    # Common ports for STM32 CDC
    for i in range(10):
        p = f'/dev/ttyACM{i}'
        if os.path.exists(p):
            return p
    return None

def main():
    port = find_serial_port()
    if not port:
        print("Error: Could not find serial port.")
        sys.exit(1)

    print(f"Connecting to {port} at {BAUD}...")
    try:
        ser = serial.Serial(port, BAUD, timeout=1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

    print(f"Logging to {LOG_FILE}...")
    
    with open(LOG_FILE, 'w') as f:
        # Header for CSV
        f.write("timestamp,Vin_mV,Vout_mV,Ain_mA,Aout_mA,Win_mW,Wout_mW,duty_x100,mppt_step,state,fault\n")
        
        try:
            while True:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if not line:
                    continue
                
                # Check for JSON telemetry
                if line.startswith('{"type":"telemetry"'):
                    try:
                        data = json.loads(line)
                        ts = time.time()
                        
                        csv_line = f"{ts},{data['Vin_mV']},{data['Vout_mV']},{data['Ain_mA']},{data['Aout_mA']}," \
                                   f"{data['Win_mW']},{data['Wout_mW']},{data['duty_x100']},{data['mppt_step']}," \
                                   f"{data['state']},{data['fault_reason']}\n"
                        f.write(csv_line)
                        f.flush()
                        
                        # Console summary
                        print(f"[{data['state']}] Vin: {data['Vin_mV']}mV, Iout: {data['Aout_mA']}mA, Duty: {data['duty_x100']}, Fault: {data['fault_reason']}")
                        
                        if data['fault_reason'] != 'NONE':
                            print(f"!!! FAULT DETECTED: {data['fault_reason']} !!!")
                            
                    except Exception as e:
                        print(f"Parse error: {e}")
                
                # Also capture debug prints
                elif line.startswith('STATE:'):
                    print(f"EVENT: {line}")
                    
        except KeyboardInterrupt:
            print("\nStopping...")
        finally:
            ser.close()

if __name__ == "__main__":
    main()
