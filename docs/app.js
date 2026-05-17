let port;
let reader;
let writer;
let inputDone;
let outputDone;
let inputStream;
let outputStream;

const connectBtn = document.getElementById('connectBtn');
const status = document.getElementById('status');
const log = document.getElementById('log');

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
        readLoop();
    } catch (err) {
        console.error('Connection failed:', err);
        appendToLog('ERROR: ' + err.message);
    }
}

async function disconnect() {
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
    appendToLog(line);
    
    if (line.startsWith('{')) {
        try {
            const data = JSON.parse(line);
            if (data.type === 'telemetry') {
                updateTelemetryUI(data);
            }
        } catch (e) {
            console.error('JSON parse error:', e);
        }
    } else if (line.startsWith('ACK:')) {
        handleAck(line);
    }
}

function updateTelemetryUI(data) {
    for (const key in data) {
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
