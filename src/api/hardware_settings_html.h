#ifndef HARDWARE_SETTINGS_HTML_H
#define HARDWARE_SETTINGS_HTML_H

const char HARDWARE_SETTINGS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Hardware Settings - ESP32 SignalK</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #f5f5f5; padding: 20px; }
    .container { max-width: 700px; margin: 0 auto; background: white; border-radius: 10px; box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1); padding: 30px; }
    h1 { color: #333; margin-bottom: 10px; font-size: 28px; }
    .nav { margin-bottom: 30px; padding-bottom: 20px; border-bottom: 2px solid #f0f0f0; }
    .nav a { color: #667eea; text-decoration: none; margin-right: 20px; font-weight: 500; }
    h2 { color: #333; font-size: 20px; margin-bottom: 20px; }
    .form-group { margin-bottom: 20px; }
    .form-row { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; margin-bottom: 20px; }
    label { display: block; color: #333; margin-bottom: 8px; font-weight: 500; font-size: 14px; }
    input[type="number"] { width: 100%; padding: 12px 15px; border: 2px solid #e0e0e0; border-radius: 6px; font-size: 16px; }
    input:focus { outline: none; border-color: #667eea; }
    button { padding: 12px 24px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; border: none; border-radius: 6px; font-size: 16px; cursor: pointer; }
    button:hover { opacity: 0.9; }
    .message { padding: 12px; border-radius: 6px; margin-bottom: 20px; font-size: 14px; display: none; }
    .message.success { background: #d4edda; color: #155724; display: block; }
    .message.error { background: #f8d7da; color: #721c24; display: block; }
    .message.warning { background: #fff3cd; color: #856404; display: block; }
    .section { background: #f9f9f9; padding: 20px; border-radius: 6px; margin-bottom: 20px; }
    .section h3 { color: #555; font-size: 16px; margin-bottom: 15px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>Hardware Settings</h1>
    <div class="nav">
      <a href="/">Dashboard</a>
      <a href="/admin">Admin</a>
      <a href="/config">TCP Config</a>
      <a href="/settings">Settings</a>
      <a href="/hardware-settings">Hardware</a>
      <a href="/ap-settings">WiFi AP</a>
    </div>
    <div id="message" class="message"></div>
    <form id="hwForm">
      <div class="section">
        <h3>GPS Module</h3>
        <div class="form-row">
          <div class="form-group"><label>RX Pin</label><input type="number" id="gps_rx" required min="0" max="39"></div>
          <div class="form-group"><label>TX Pin</label><input type="number" id="gps_tx" required min="0" max="39"></div>
        </div>
        <div class="form-group"><label>Baud Rate</label><input type="number" id="gps_baud" required value="9600"></div>
      </div>
      <div class="section">
        <h3>RS485 (NMEA)</h3>
        <div class="form-row">
          <div class="form-group"><label>RX Pin</label><input type="number" id="rs485_rx" required min="0" max="39"></div>
          <div class="form-group"><label>TX Pin</label><input type="number" id="rs485_tx" required min="0" max="39"></div>
        </div>
        <div class="form-row">
          <div class="form-group"><label>DE Pin</label><input type="number" id="rs485_de" required min="0" max="39"></div>
          <div class="form-group"><label>DE Enable (0/1)</label><input type="number" id="rs485_de_enable" required min="0" max="1"></div>
        </div>
        <div class="form-group"><label>Baud Rate</label><input type="number" id="rs485_baud" required value="38400"></div>
      </div>
      <div class="section">
        <h3>Seatalk1</h3>
        <div class="form-group"><label>RX Pin</label><input type="number" id="seatalk1_rx" required min="0" max="39"></div>
        <div class="form-group"><label>Baud Rate</label><input type="number" id="seatalk1_baud" required value="4800"></div>
      </div>
      <div class="section">
        <h3>CAN Bus</h3>
        <div class="form-row">
          <div class="form-group"><label>RX Pin</label><input type="number" id="can_rx" required min="0" max="39"></div>
          <div class="form-group"><label>TX Pin</label><input type="number" id="can_tx" required min="0" max="39"></div>
        </div>
      </div>
      <button type="submit">Save Hardware Settings</button>
    </form>
  </div>
  <script>
function showMessage(t,e){const s=document.getElementById("message");s.textContent=t,s.className="message "+e,setTimeout(()=>{s.className="message"},5e3)}async function load(){try{const t=await fetch("/api/settings/hardware"),e=await t.json();document.getElementById("gps_rx").value=e.gps.rx,document.getElementById("gps_tx").value=e.gps.tx,document.getElementById("gps_baud").value=e.gps.baud,document.getElementById("rs485_rx").value=e.rs485.rx,document.getElementById("rs485_tx").value=e.rs485.tx,document.getElementById("rs485_de").value=e.rs485.de,document.getElementById("rs485_de_enable").value=e.rs485.de_enable,document.getElementById("rs485_baud").value=e.rs485.baud,document.getElementById("seatalk1_rx").value=e.seatalk1.rx,document.getElementById("seatalk1_baud").value=e.seatalk1.baud,document.getElementById("can_rx").value=e.can.rx,document.getElementById("can_tx").value=e.can.tx}catch(t){showMessage("Failed to load settings","error")}}document.getElementById("hwForm").addEventListener("submit",async t=>{t.preventDefault();const e={gps:{rx:parseInt(document.getElementById("gps_rx").value),tx:parseInt(document.getElementById("gps_tx").value),baud:parseInt(document.getElementById("gps_baud").value)},rs485:{rx:parseInt(document.getElementById("rs485_rx").value),tx:parseInt(document.getElementById("rs485_tx").value),de:parseInt(document.getElementById("rs485_de").value),de_enable:parseInt(document.getElementById("rs485_de_enable").value),baud:parseInt(document.getElementById("rs485_baud").value)},seatalk1:{rx:parseInt(document.getElementById("seatalk1_rx").value),baud:parseInt(document.getElementById("seatalk1_baud").value)},can:{rx:parseInt(document.getElementById("can_rx").value),tx:parseInt(document.getElementById("can_tx").value)}};try{const t=await fetch("/api/settings/hardware",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(e)}),s=await t.json();t.ok?showMessage("Settings saved! Restart required.","warning"):showMessage(s.error||"Failed to save","error")}catch(t){showMessage("Network error","error")}}),load();
  </script>
</body>
</html>
)rawliteral";

#endif
