#ifndef HTML_PAGE_H
#define HTML_PAGE_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32-CAM Monitor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f0f0f0; }
    .container { max-width: 1200px; margin: 0 auto; background: white; border-radius: 10px; padding: 20px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
    h1 { text-align: center; color: #333; }
    .video-container { width: 100%; background: #000; border-radius: 10px; overflow: hidden; margin-bottom: 20px; }
    #stream { width: 100%; max-height: 400px; }
    .control-panel { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin: 20px 0; }
    @media (max-width: 768px) { .control-panel { grid-template-columns: 1fr; } }
    .panel { background: #f8f9fa; padding: 20px; border-radius: 8px; border: 1px solid #dee2e6; }
    .btn { background: #007bff; color: white; border: none; padding: 12px 20px; border-radius: 6px; cursor: pointer; font-size: 16px; width: 100%; margin: 5px 0; transition: background 0.3s; }
    .btn:hover { background: #0056b3; }
    .btn-capture { background: #28a745; }
    .btn-capture:hover { background: #1e7e34; }
    .btn-test { background: #ffc107; color: #212529; }
    .btn-test:hover { background: #e0a800; }
    .stats-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 15px; margin: 20px 0; }
    .stat-card { background: #17a2b8; color: white; padding: 15px; border-radius: 8px; text-align: center; }
    .stat-value { font-size: 1.5em; font-weight: bold; margin: 5px 0; }
    .log-section { background: #343a40; color: white; padding: 15px; border-radius: 8px; margin: 20px 0; font-family: monospace; font-size: 14px; }
    .debug-section { background: #6c757d; color: white; padding: 15px; border-radius: 8px; margin: 20px 0; font-family: monospace; font-size: 12px; max-height: 240px; overflow-y: auto; }
    .form-group { margin-bottom: 15px; }
    label { display: block; margin-bottom: 5px; font-weight: bold; }
    select, input { width: 100%; padding: 8px; border: 1px solid #ccc; border-radius: 4px; box-sizing: border-box; }
    .hint { font-size: 12px; opacity: 0.85; margin-top: 6px; }
  </style>
</head>
<body>
<div class="container">
  <h1>ESP32-CAM Security Monitor</h1>

  <div class="video-container">
    <img id="stream" src="/stream">
  </div>

  <div class="stats-grid">
    <div class="stat-card"><div>Captured Images</div><div id="capturedCount" class="stat-value">0</div></div>
    <div class="stat-card"><div>Sent to Telegram</div><div id="sentCount" class="stat-value">0</div></div>
  </div>

  <div class="control-panel">
    <div class="panel">
      <h3>Manual Control</h3>
      <button class="btn btn-capture" onclick="captureNow()">Capture Image Now</button>
      <button class="btn btn-test" onclick="testTelegram()">Test Telegram</button>
      <button class="btn" onclick="refreshStream()">Refresh Stream</button>
      <button class="btn" onclick="openDebug()">Open /debug</button>
      <div class="hint">Tip: while editing settings, auto-refresh won’t override your inputs.</div>
    </div>

    <div class="panel">
      <h3>Automation Settings</h3>
      <div class="form-group">
        <label>Capture Mode:</label>
        <select id="captureMode" onchange="updateMode()">
          <option value="0">Motion Detection</option>
          <option value="1">Time Based</option>
          <option value="2">Mixed Mode</option>
        </select>
      </div>

      <div class="form-group">
        <label>Time Interval (minutes):</label>
        <input type="number" id="timeInterval" min="1" max="1000" value="5">
      </div>

      <div class="form-group">
        <label>Motion Sensitivity:</label>
        <input type="range" id="motionThreshold" min="1000" max="20000" value="5000" step="1000">
        <div>Sensitivity: <span id="sensitivityValue">Medium</span></div>
      </div>

      <button class="btn" onclick="saveSettings()">Save Settings</button>
      <div class="hint">Settings are applied immediately and will be persisted shortly (throttled EEPROM write).</div>
    </div>
  </div>

  <div class="log-section">
    <h3>Activity Log</h3>
    <div><strong>Last Capture:</strong> <span id="lastCaptureTime">Never</span></div>
    <div><strong>Last Capture Type:</strong> <span id="lastCaptureType">None</span></div>
    <div><strong>Last Telegram Result:</strong> <span id="lastTelegramResult">Never</span></div>
    <div><strong>Current Mode:</strong> <span id="currentMode">Motion Detection</span></div>
    <div><strong>Device Uptime:</strong> <span id="uptime">0s</span></div>
  </div>

  <div class="debug-section">
    <h3>Telegram Debug</h3>
    <div id="telegramDebug">No debug info yet...</div>
  </div>
</div>

<script>
  // ✅ prevents /status polling from overwriting user inputs while editing
  let isEditing = false;
  let editTimer = null;

  function setEditing(on) {
    isEditing = on;
    if (editTimer) clearTimeout(editTimer);
    if (on) {
      // auto-release edit mode after user stops interacting
      editTimer = setTimeout(() => { isEditing = false; }, 6000);
    }
  }

  function updateMode() {
    const mode = document.getElementById('captureMode').value;
    document.getElementById('currentMode').textContent =
      ['Motion Detection', 'Time Based', 'Mixed Mode'][mode];
  }

  function updateSensitivity() {
    const value = parseInt(document.getElementById('motionThreshold').value, 10);
    document.getElementById('sensitivityValue').textContent =
      value < 5000 ? 'High' : value < 10000 ? 'Medium' : 'Low';
  }

  function captureNow() {
    fetch('/capture-now')
      .then(r => r.text())
      .then(result => { alert('Capture: ' + result); updateStatus(); });
  }

  function testTelegram() {
    fetch('/test-telegram')
      .then(r => r.text())
      .then(result => { alert('Test: ' + result); updateStatus(); });
  }

  function refreshStream() {
    document.getElementById('stream').src = '/stream?t=' + Date.now();
  }

  function openDebug() {
    window.open('/debug', '_blank');
  }

  function saveSettings() {
    setEditing(false);

    const settings = {
      mode: parseInt(document.getElementById('captureMode').value, 10),
      interval: parseInt(document.getElementById('timeInterval').value, 10),
      threshold: parseInt(document.getElementById('motionThreshold').value, 10)
    };

    fetch('/save-settings', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(settings)
    }).then(r => r.text()).then(result => {
      alert('Saved: ' + result);
      updateStatus();
    });
  }

  function updateStatus() {
    fetch('/status')
      .then(r => r.json())
      .then(data => {
        document.getElementById('capturedCount').textContent = data.capturedCount;
        document.getElementById('sentCount').textContent = data.sentCount;
        document.getElementById('lastCaptureTime').textContent = data.lastCaptureTime;
        document.getElementById('lastCaptureType').textContent = data.lastCaptureType;
        document.getElementById('lastTelegramResult').textContent = data.lastTelegramResult;
        document.getElementById('currentMode').textContent = data.currentMode;
        document.getElementById('telegramDebug').textContent = data.telegramDebug;
        document.getElementById('uptime').textContent = data.uptime;

        // ✅ don't overwrite the form while user is editing
        if (!isEditing) {
          document.getElementById('captureMode').value = data.captureMode;
          document.getElementById('timeInterval').value = data.timeInterval;
          document.getElementById('motionThreshold').value = data.motionThreshold;
          updateSensitivity();
          updateMode();
        }
      })
      .catch(() => {});
  }

  // Mark editing on interaction
  const modeEl = document.getElementById('captureMode');
  const intervalEl = document.getElementById('timeInterval');
  const thrEl = document.getElementById('motionThreshold');

  [modeEl, intervalEl, thrEl].forEach(el => {
    el.addEventListener('focus', () => setEditing(true));
    el.addEventListener('input', () => setEditing(true));
    el.addEventListener('change', () => setEditing(true));
    el.addEventListener('blur', () => setEditing(false));
  });

  document.getElementById('motionThreshold').addEventListener('input', updateSensitivity);

  // ✅ less load, and no stream refresh while editing
  setInterval(updateStatus, 4000);
  setInterval(() => {
    if (!isEditing) refreshStream();
  }, 2500);

  updateStatus(); updateSensitivity(); updateMode();
</script>
</body>
</html>
)rawliteral";

#endif
