#ifndef AP_SETTINGS_HTML_H
#define AP_SETTINGS_HTML_H

const char AP_SETTINGS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>WiFi AP Settings - ESP32 SignalK</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #f5f5f5; padding: 20px; }
    .container { max-width: 600px; margin: 0 auto; background: white; border-radius: 10px; box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1); padding: 30px; }
    h1 { color: #333; margin-bottom: 10px; font-size: 28px; }
    .nav { margin-bottom: 30px; padding-bottom: 20px; border-bottom: 2px solid #f0f0f0; }
    .nav a { color: #667eea; text-decoration: none; margin-right: 20px; font-weight: 500; }
    h2 { color: #333; font-size: 20px; margin-bottom: 20px; }
    .form-group { margin-bottom: 20px; }
    label { display: block; color: #333; margin-bottom: 8px; font-weight: 500; font-size: 14px; }
    input[type="text"], input[type="password"] { width: 100%; padding: 12px 15px; border: 2px solid #e0e0e0; border-radius: 6px; font-size: 16px; }
    input:focus { outline: none; border-color: #667eea; }
    button { padding: 12px 24px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; border: none; border-radius: 6px; font-size: 16px; cursor: pointer; margin-right: 10px; }
    button:hover { opacity: 0.9; }
    .danger-btn { background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%); }
    .message { padding: 12px; border-radius: 6px; margin-bottom: 20px; font-size: 14px; display: none; }
    .message.success { background: #d4edda; color: #155724; display: block; }
    .message.error { background: #f8d7da; color: #721c24; display: block; }
    .message.warning { background: #fff3cd; color: #856404; display: block; }
    .info { background: #e7f3ff; padding: 15px; border-radius: 6px; margin-bottom: 20px; font-size: 14px; color: #004085; }
  </style>
</head>
<body>
  <div class="container">
    <h1>WiFi AP Settings</h1>
    <div class="nav">
      <a href="/">Dashboard</a>
      <a href="/admin">Admin</a>
      <a href="/config">TCP Config</a>
      <a href="/settings">Settings</a>
      <a href="/hardware-settings">Hardware</a>
      <a href="/ap-settings">WiFi AP</a>
    </div>
    <div id="message" class="message"></div>
    <div class="info">
      Current AP SSID: <strong id="current-ssid">Loading...</strong><br>
      To change WiFi AP credentials, use the Reset WiFi button below. The device will restart and create a configuration portal.
    </div>
    <h2>WiFi Configuration</h2>
    <form id="apForm">
      <div class="form-group">
        <label>New AP SSID (optional, 1-32 chars)</label>
        <input type="text" id="ap_ssid" minlength="1" maxlength="32" placeholder="Leave empty to keep current">
      </div>
      <div class="form-group">
        <label>New AP Password (optional, 8-63 chars)</label>
        <input type="password" id="ap_password" minlength="8" maxlength="63" placeholder="Leave empty to keep current">
      </div>
      <div class="form-group">
        <label>Confirm Password</label>
        <input type="password" id="ap_password_confirm" minlength="8" maxlength="63">
      </div>
      <button type="submit">Save AP Settings</button>
      <button type="button" class="danger-btn" onclick="resetWiFi()">Reset WiFi & Restart</button>
    </form>
  </div>
  <script>
function showMessage(t,e){const s=document.getElementById("message");s.textContent=t,s.className="message "+e,setTimeout(()=>{s.className="message"},5e3)}async function load(){try{const t=await fetch("/api/settings/ap"),e=await t.json();document.getElementById("current-ssid").textContent=e.ssid||"ESP32-SignalK"}catch(t){document.getElementById("current-ssid").textContent="Unknown"}}async function resetWiFi(){if(!confirm("This will erase WiFi credentials and restart the device. The AP configuration portal will start. Continue?"))return;try{const t=await fetch("/api/wifi/reset",{method:"POST"});if(t.ok)showMessage("WiFi reset! Device restarting...","warning"),setTimeout(()=>window.location.href="/",5e3);else{const e=await t.json();showMessage(e.error||"Failed to reset WiFi","error")}}catch(t){showMessage("Network error","error")}}document.getElementById("apForm").addEventListener("submit",async t=>{t.preventDefault();const e=document.getElementById("ap_ssid").value,s=document.getElementById("ap_password").value,a=document.getElementById("ap_password_confirm").value;if(s&&s!==a)return void showMessage("Passwords do not match","error");if(s&&s.length<8)return void showMessage("Password must be at least 8 characters","error");if(!e&&!s)return void showMessage("Please enter SSID and/or password","warning");try{const t=await fetch("/api/settings/ap",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({ssid:e||null,password:s||null})}),a=await t.json();t.ok?showMessage("AP settings saved! RESTART REQUIRED.","warning"):showMessage(a.error||"Failed to save","error")}catch(t){showMessage("Network error","error")}}),load();
  </script>
</body>
</html>
)rawliteral";

#endif
