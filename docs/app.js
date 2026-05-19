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
const startVoltCalBtn = document.getElementById('startVoltCalBtn');
const startCurrCalBtn = document.getElementById('startCurrCalBtn');
const wizardContainer = document.getElementById('wizard-container');

// Limits buttons
const saveLimitsBtn = document.getElementById('saveLimitsBtn');

let currentWizMode = 'v'; // 'v' or 'i'
let currentWizStep = 1;

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
        if (key.endsWith('_limit')) continue;

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
    // Logic is now unified via updateTelemetryUI style, but we'll manually set here too for GET_LIMITS
    if (data.Vmax_limit !== undefined) document.getElementById('limit_Vmax').value = (data.Vmax_limit / 1000).toFixed(2);
    if (data.Vmin_limit !== undefined) document.getElementById('limit_Vmin').value = (data.Vmin_limit / 1000).toFixed(2);
    if (data.Imax_limit !== undefined) document.getElementById('limit_Imax').value = (data.Imax_limit / 1000).toFixed(2);
    appendToLog('Limits synchronized');
}

function updateCalibrationUI(data) {
    // Route raw ADC values to wizard if open
    if (wizardContainer.style.display === 'flex') {
        const ids = [
            'wiz_Vin_raw_v1', 'wiz_Vout_raw_v1',
            'wiz_Vin_raw_v2', 'wiz_Vout_raw_v2',
            'wiz_Ain_raw_i1', 'wiz_Aout_raw_i1',
            'wiz_Ain_raw_i2', 'wiz_Aout_raw_i2'
        ];
        
        ids.forEach(id => {
            const el = document.getElementById(id);
            if (el) {
                if (id.includes('Vin')) el.textContent = data.Vin_raw;
                if (id.includes('Vout')) el.textContent = data.Vout_raw;
                if (id.includes('Ain')) el.textContent = data.Ain_raw;
                if (id.includes('Aout')) el.textContent = data.Aout_raw;
            }
        });
    }
}

function handleAck(line) {
    // Acks can be used for UI confirmation if needed
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
    const valueInt = Math.round(parseFloat(value) * 1000);
    await sendCommand(`CMD:${cmdPrefix}:${valueInt}`);
};

// Wizard Handlers
startVoltCalBtn.addEventListener('click', () => {
    if (!port) {
        alert('Please connect to the device first.');
        return;
    }
    currentWizMode = 'v';
    currentWizStep = 1;
    sendCommand('CMD:CAL_ENTER');
    sendCommand('CMD:CAL_MODE_V');
    wizShowStep('v', 1);
    wizardContainer.style.display = 'flex';
});

startCurrCalBtn.addEventListener('click', () => {
    if (!port) {
        alert('Please connect to the device first.');
        return;
    }
    currentWizMode = 'i';
    currentWizStep = 1;
    sendCommand('CMD:CAL_ENTER');
    // We don't send MODE_I yet, only after intro step
    wizShowStep('i', 1);
    wizardContainer.style.display = 'flex';
});

window.wizShowStep = (mode, step) => {
    document.querySelectorAll('.wizard-step').forEach(el => el.style.display = 'none');
    const stepEl = document.getElementById(`step-${mode}-${step}`);
    if (stepEl) stepEl.style.display = 'block';
    currentWizMode = mode;
    currentWizStep = step;
};

window.wizEnableCurrentModeFlow = async () => {
    await sendCommand('CMD:CAL_MODE_I');
    wizShowStep('i', 2);
};

window.wizClose = async () => {
    if (confirm('Are you sure you want to abort calibration?')) {
        await sendCommand('CMD:CAL_EXIT');
        wizardContainer.style.display = 'none';
    }
};

window.wizRecord = async (point) => {
    let inputId, cmdPrefix, multiplier;
    
    if (point.startsWith('V')) {
        inputId = point === 'V_LOW' ? 'wiz_V_low' : 'wiz_V_high';
        cmdPrefix = 'CMD:CAL_' + point;
        multiplier = 1000;
    } else {
        inputId = point === 'I_LOW' ? 'wiz_I_low' : 'wiz_I_high';
        cmdPrefix = 'CMD:CAL_' + point;
        multiplier = 1000;
    }

    const value = document.getElementById(inputId).value;
    const valueInt = Math.round(parseFloat(value) * multiplier);
    await sendCommand(`${cmdPrefix}:${valueInt}`);

    wizShowStep(currentWizMode, currentWizStep + 1);
};

window.wizFinish = async () => {
    await sendCommand('CMD:CAL_SAVE');
    await sendCommand('CMD:CAL_EXIT');
    wizardContainer.style.display = 'none';
    alert('Calibration saved successfully!');
};

saveLimitsBtn.addEventListener('click', () => sendCommand('CMD:LIMITS_SAVE'));
resetFaultBtn.addEventListener('click', () => sendCommand('CMD:RESET_FAULT'));
