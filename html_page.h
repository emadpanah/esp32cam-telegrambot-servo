// html_page.h  (ONE SERVO VERSION)
#ifndef HTML_PAGE_H
#define HTML_PAGE_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8"/>
  <title>ESP32-CAM Servo Monitor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <style>
    body { font-family: Arial, sans-serif; margin:0; padding:16px; background:#f3f3f3; }
    .wrap { max-width:1100px; margin:0 auto; background:#fff; padding:16px; border-radius:12px; box-shadow:0 2px 10px rgba(0,0,0,.08);}
    h1 { margin:0 0 12px; }
    .row { display:grid; grid-template-columns: 1fr 1fr; gap:14px; }
    @media(max-width:860px){ .row{grid-template-columns:1fr;} }
    .card { background:#fafafa; border:1px solid #e6e6e6; border-radius:10px; padding:14px; }
    .video { background:#000; border-radius:10px; overflow:hidden; }
    #stream { width:100%; max-height:420px; display:block; }
    .btn { width:100%; border:0; border-radius:8px; padding:12px; margin:6px 0; cursor:pointer; font-size:15px; }
    .btn.primary { background:#1d72b8; color:#fff; }
    .btn.good { background:#28a745; color:#fff; }
    .btn.warn { background:#ffc107; color:#111; }
    .btn.dark { background:#343a40; color:#fff; }
    label { display:block; font-weight:700; margin:10px 0 6px; }
    input, select { width:100%; padding:9px; border-radius:7px; border:1px solid #ccc; box-sizing:border-box; }
    .grid2 { display:grid; grid-template-columns:1fr 1fr; gap:10px; }
    .stat { background:#17a2b8; color:#fff; border-radius:10px; padding:12px; text-align:center; }
    .stat b { font-size:22px; display:block; margin-top:4px; }
    .mono { font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace; }
    .log { background:#111; color:#eaeaea; border-radius:10px; padding:12px; }
    .small { font-size:12px; opacity:.85; }
    .servoBtns { display:grid; grid-template-columns:1fr 1fr 1fr; gap:8px; }
  </style>
</head>
<body>
  <div class="wrap">
    <h1>ESP32-CAM Servo Monitor (1-Servo)</h1>

    <div class="video">
      <img id="stream" src="/stream">
    </div>

    <div class="grid2" style="margin-top:14px;">
      <div class="stat">Captured<b id="capturedCount">0</b></div>
      <div class="stat">Sent<b id="sentCount">0</b></div>
    </div>

    <div class="row" style="margin-top:14px;">
      <div class="card">
        <h3>Manual</h3>
        <button class="btn good" onclick="captureNow()">Capture Now</button>
        <button class="btn warn" onclick="testTelegram()">Test Telegram</button>
        <button class="btn primary" onclick="refreshStream()">Refresh Stream</button>
        <button class="btn dark" onclick="openDebug()">Open /debug</button>
      </div>

      <div class="card">
        <h3>Automation</h3>
        <label>Capture Mode</label>
        <select id="captureMode">
          <option value="0">Motion</option>
          <option value="1">Time</option>
          <option value="2">Mixed</option>
        </select>

        <label>Time Interval (minutes)</label>
        <input id="timeInterval" type="number" min="1" max="1000"/>

        <label>Motion Sensitivity</label>
        <input id="motionThreshold" type="range" min="1000" max="20000" step="1000"/>
        <div class="small">Value: <span id="thVal">0</span></div>

        <button class="btn primary" onclick="saveSettings()">Save Settings</button>
      </div>
    </div>

    <div class="row" style="margin-top:14px;">
      <div class="card">
        <h3>Servo (Pan)</h3>
        <div class="servoBtns">
          <button class="btn primary" onclick="servoCmd('left')">⬅️ Left</button>
          <button class="btn primary" onclick="servoCmd('center')">🎯 Center</button>
          <button class="btn primary" onclick="servoCmd('right')">➡️ Right</button>
        </div>

        <label>Pan (0..180)</label>
        <input id="pan" type="number" min="0" max="180"/>
        <button class="btn good" onclick="setPan()">Apply Pan</button>

        <div class="small mono">Pan=<span id="panVal">?</span></div>
      </div>

      <div class="card">
        <h3>Status</h3>
        <div class="log mono">
          <div><b>Last Capture:</b> <span id="lastCaptureTime">-</span></div>
          <div><b>Type:</b> <span id="lastCaptureType">-</span></div>
          <div><b>Telegram:</b> <span id="lastTelegramResult">-</span></div>
          <div><b>Mode:</b> <span id="currentMode">-</span></div>
          <div><b>Uptime:</b> <span id="uptime">-</span></div>
          <div><b>Now:</b> <span id="nowTime">-</span></div>
          <hr style="border:0;border-top:1px solid #333;opacity:.35"/>
          <div class="small"><b>Debug:</b></div>
          <div id="telegramDebug" class="small">-</div>
        </div>
      </div>
    </div>
  </div>

<script>
function refreshStream(){ document.getElementById('stream').src = '/stream?t=' + Date.now(); }
function openDebug(){ window.open('/debug','_blank'); }

function captureNow(){ fetch('/capture-now').then(r=>r.text()).then(x=>alert(x)); }
function testTelegram(){ fetch('/test-telegram').then(r=>r.text()).then(x=>alert(x)); }

function saveSettings(){
  const body = {
    mode: parseInt(document.getElementById('captureMode').value,10),
    interval: parseInt(document.getElementById('timeInterval').value,10),
    threshold: parseInt(document.getElementById('motionThreshold').value,10)
  };
  fetch('/save-settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
    .then(r=>r.text()).then(x=>alert(x));
}

function servoCmd(cmd){
  fetch('/servo?cmd=' + encodeURIComponent(cmd)).then(r => r.text()).then(_ => {});
}
function setPan(){
  const pan = parseInt(document.getElementById('pan').value||'0',10);
  fetch('/servo?pan=' + pan).then(r=>r.text()).then(_ => {});
}

function updateUI(data){
  document.getElementById('capturedCount').textContent = data.capturedCount;
  document.getElementById('sentCount').textContent = data.sentCount;
  document.getElementById('lastCaptureTime').textContent = data.lastCaptureTime;
  document.getElementById('lastCaptureType').textContent = data.lastCaptureType;
  document.getElementById('lastTelegramResult').textContent = data.lastTelegramResult;
  document.getElementById('currentMode').textContent = data.currentMode;
  document.getElementById('uptime').textContent = data.uptime;
  document.getElementById('nowTime').textContent = data.nowTime;
  document.getElementById('telegramDebug').textContent = data.telegramDebug;

  document.getElementById('captureMode').value = data.captureMode;
  document.getElementById('timeInterval').value = data.timeInterval;
  document.getElementById('motionThreshold').value = data.motionThreshold;
  document.getElementById('thVal').textContent = data.motionThreshold;

  document.getElementById('panVal').textContent = data.pan;
  if(document.activeElement.id !== 'pan') document.getElementById('pan').value = data.pan;
}

function tick(){
  fetch('/status').then(r=>r.json()).then(updateUI).catch(()=>{});
}
setInterval(tick, 2000);
setInterval(()=>{ document.getElementById('stream').src='/stream?t='+Date.now(); }, 1500);
tick();
</script>
</body>
</html>
)rawliteral";

#endif
