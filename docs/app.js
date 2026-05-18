let port;
let reader;
let writer;
let inputDone;
let outputDone;
let inputStream;
let outputStream;
let syncInterval;

const connectBtn = document.getElementById('connectBtn');
const status = document.getElementById('status');
const log = document.getElementById('log');
const resetFaultBtn = document.getElementById('resetFaultBtn');
const faultReasonSpan = document.getElementById('fault-reason');

// Calibration buttons
const calEnterBtn = document.getElementById('calEnterBtn');
const calExitBtn = document.getElementById('calExitBtn');
const calControls = document.getElementById('calControls');
const calSaveBtn = document.getElementById('calSaveBtn');

// Limits buttons
const saveLimitsBtn = document.getElementById('saveLimitsBtn');

connectBtn.addEventListener('click', async () => {
    if (port) {
        await disconnect();
    } else {
        await connect();
    }
});

async function connect() {
    try {
        port = await navigator.serial.requestPort();
        await port.open({ baudRate: 115200 });

        status.textContent = 'Connected';
        status.className = 'connected';
        connectBtn.textContent = 'Disconnect';

        const encoder = new TextEncoderStream();
        outputDone = encoder.readable.pipeTo(port.writable);
        outputStream = encoder.writable;
        writer = outputStream.getWriter();

        const decoder = new TextDecoderStream();
        inputDone = port.readable.pipeTo(decoder.writable);
        inputStream = decoder.readable;

        reader = inputStream.getReader();
        document.getElementById('state-badge').style.display = 'inline-block';
        
        // Robust fetch: Retry every 2s until success
        if (syncInterval) clearInterval(syncInterval);
        syncInterval = setInterval(() => sendCommand('CMD:GET_LIMITS'), 2000);
        // Also try once immediately after a short grace period
        setTimeout(() => sendCommand('CMD:GET_LIMITS'), 500);
        
        readLoop();
    } catch (err) {
        console.error('Connection failed:', err);
        appendToLog('ERROR: ' + err.message);
    }
}

async function disconnect() {
    if (syncInterval) {
        clearInterval(syncInterval);
        syncInterval = null;
    }
    if (reader) {
        await reader.cancel();
        await inputDone.catch(() => {});
        reader = null;
        inputDone = null;
    }
    if (writer) {
        await writer.close();
        await outputDone;
        writer = null;
        outputDone = null;
    }
    await port.close();
    port = null;

    status.textContent = 'Disconnected';
    status.className = 'disconnected';
    document.getElementById('state-badge').style.display = 'none';
    connectBtn.textContent = 'Connect to Device';
}

async function readLoop() {
    let buffer = '';
    while (true) {
        try {
            const { value, done } = await reader.read();
            if (done) {
                console.log('Reader done');
                break;
            }
            
            buffer += value;
            let lines = buffer.split('\n');
            buffer = lines.pop(); // Keep the last partial line

            for (let line of lines) {
                line = line.trim();
                if (line) {
                    processLine(line);
                }
            }
        } catch (err) {
            console.error('Read error:', err);
            appendToLog('READ ERROR: ' + err.message);
            break;
        }
    }
}

function processLine(line) {
    if (!line.startsWith('{')) {
        appendToLog(line);
    }
    
    if (line.startsWith('{')) {
        try {
            const data = JSON.parse(line);
            if (data.type === 'telemetry') {
                updateTelemetryUI(data);
            } else if (data.type === 'cal_raw') {
                updateCalibrationUI(data);
            } else if (data.type === 'limits') {
                updateLimitsUI(data);
            }
        } catch (e) {
            console.error('JSON parse error:', e);
            appendToLog(line);
        }
    } else if (line.startsWith('ACK:')) {
        handleAck(line);
    }
}

function updateTelemetryUI(data) {
    for (const key in data) {
        if (key === 'type') continue;
        
        if (key === 'state') {
            const badge = document.getElementById('state-badge');
            if (badge) {
                badge.textContent = data[key];
                badge.className = '';
                badge.classList.add('state-' + data[key].toLowerCase());
            }

            if (data[key] === 'FAULT') {
                resetFaultBtn.style.display = 'inline-block';
                faultReasonSpan.style.display = 'inline-block';
                faultReasonSpan.textContent = data.fault_reason || 'UNKNOWN';
            } else {
                resetFaultBtn.style.display = 'none';
                faultReasonSpan.style.display = 'none';
            }
            continue;
        }

        if (key === 'fault_reason') continue;
        if (key === 'V_limit' || key === 'I_limit') continue;

        const el = document.getElementById(key);
        if (el) {
            let val = data[key];
            if (key.endsWith('_mV')) {
                el.textContent = (val / 1000).toFixed(2) + ' V';
            } else if (key.endsWith('_mA')) {
                el.textContent = (val / 1000).toFixed(2) + ' A';
            } else if (key.endsWith('_mW')) {
                el.textContent = (val / 1000).toFixed(2) + ' W';
            } else if (key === 'eff') {
                el.textContent = val + ' %';
            } else if (key === 'temp_C') {
                el.textContent = val + ' °C';
            } else if (key === 'duty_x100') {
                el.textContent = (val / 100).toFixed(2) + ' %';
            } else {
                el.textContent = val;
            }
        }
    }
}

function updateLimitsUI(data) {
    if (syncInterval) {
        clearInterval(syncInterval);
        syncInterval = null;
    }
    if (data.Vmax) document.getElementById('limit_Vmax').value = data.Vmax;
    if (data.Vmin) document.getElementById('limit_Vmin').value = data.Vmin;
    if (data.Imax) document.getElementById('limit_Imax').value = data.Imax;
    appendToLog('Limits fetched from device');
}

function updateCalibrationUI(data) {
    for (const key in data) {
        if (key === 'type') continue;
        const el = document.getElementById(key);
        if (el) {
            el.textContent = data[key];
        }
    }
}

function handleAck(line) {
    if (line.includes('CAL_ENTER_OK')) {
        calControls.style.display = 'block';
    } else if (line.includes('CAL_EXIT_OK')) {
        calControls.style.display = 'none';
    }
}

function appendToLog(text) {
    const line = document.createElement('div');
    line.textContent = text;
    log.appendChild(line);
    log.scrollTop = log.scrollHeight;
}

async function sendCommand(cmd) {
    if (!writer) {
        alert('Please connect to the device first.');
        return;
    }
    appendToLog('> ' + cmd);
    await writer.write(cmd + '\n');
}

window.sendLimit = async (cmdPrefix, inputId) => {
    const value = document.getElementById(inputId).value;
    await sendCommand(`CMD:${cmdPrefix}:${value}`);
};

window.sendCal = async (point) => {
    const ref = document.getElementById('calRefValue').value;
    await sendCommand(`CMD:CAL_${calControls.querySelector('button.active')?.textContent.includes('Voltage') ? 'V' : 'I'}_${point}:${ref}`);
};

// Use a simpler approach for cal buttons
calEnterBtn.addEventListener('click', () => sendCommand('CMD:CAL_ENTER'));
calExitBtn.addEventListener('click', () => sendCommand('CMD:CAL_EXIT'));
calSaveBtn.addEventListener('click', () => sendCommand('CMD:CAL_SAVE'));
saveLimitsBtn.addEventListener('click', () => sendCommand('CMD:LIMITS_SAVE'));
resetFaultBtn.addEventListener('click', () => sendCommand('CMD:RESET_FAULT'));

// Minimal state for cal mode
const calModeButtons = calControls.querySelectorAll('.button-group:first-child button');
calModeButtons.forEach(btn => {
    btn.addEventListener('click', () => {
        calModeButtons.forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
    });
});
// Default active
calModeButtons[0].classList.add('active');
