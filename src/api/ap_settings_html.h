#ifndef AP_SETTINGS_HTML_H
#define AP_SETTINGS_HTML_H

const char AP_SETTINGS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>WiFi AP Settings - SignalK</title>
  <style>
    :root {
      --bg: #f5f7fb;
      --card-bg: #ffffff;
      --primary: #1f7afc;
      --primary-dark: #1554c0;
      --text: #1f2a37;
      --muted: #5d6b82;
      --border: #e4e7ec;
      --danger: #ef4444;
    }

    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: var(--bg); color: var(--text); padding: 24px; line-height: 1.45; }
    .container { max-width: 1200px; margin: 0 auto; }

    .card { background: var(--card-bg); border-radius: 18px; padding: 24px; margin-bottom: 24px; box-shadow: 0 20px 45px rgba(15, 23, 42, 0.08); }
    .hero-card { background: linear-gradient(135deg, #1f7afc, #6c5ce7); color: #fff; border: none; }
    .hero-content { display: flex; justify-content: space-between; gap: 24px; align-items: flex-start; flex-wrap: wrap; }
    .hero-text .eyebrow { text-transform: uppercase; letter-spacing: 0.08em; font-size: 11px; opacity: 0.85; margin-bottom: 8px; }
    .hero-text h1 { font-size: 30px; margin-bottom: 8px; }
    .hero-text .subtitle { font-size: 15px; color: rgba(255,255,255,0.88); max-width: 520px; }
    .hero-actions { display: flex; gap: 12px; flex-wrap: wrap; }

    .btn-link { display: inline-flex; align-items: center; justify-content: center; padding: 10px 20px; border-radius: 999px; font-weight: 600; text-decoration: none; border: 1px solid rgba(255,255,255,0.5); transition: transform 0.2s ease, box-shadow 0.2s ease; }
    .btn-link.primary { background: #fff; color: #1f2a37; border-color: transparent; }
    .btn-link.secondary { color: #fff; }
    .btn-link:hover { transform: translateY(-1px); box-shadow: 0 8px 20px rgba(15, 23, 42, 0.25); }

    h2 { font-size: 20px; margin-bottom: 6px; }
    .muted { color: var(--muted); font-size: 14px; }
    .card-header { display: flex; align-items: flex-start; justify-content: space-between; gap: 12px; margin-bottom: 12px; flex-wrap: wrap; }
    .subtle-card { background: #eef4ff; border: 1px solid #dbe4ff; color: var(--muted); }

    .form-group { margin-bottom: 18px; }
    label { display: block; color: var(--text); margin-bottom: 8px; font-weight: 600; font-size: 14px; }
    input[type="text"], input[type="password"] { width: 100%; padding: 12px; border: 1px solid var(--border); border-radius: 12px; font-size: 15px; background: #f9fafb; transition: border 0.2s ease; }
    input[type="text"]:focus, input[type="password"]:focus { border-color: var(--primary); outline: none; background: #fff; }

    .button-row { display: flex; gap: 12px; flex-wrap: wrap; margin-top: 20px; }
    .btn { border: none; border-radius: 12px; padding: 12px 18px; font-size: 15px; font-weight: 600; cursor: pointer; transition: transform 0.2s ease, box-shadow 0.2s ease; }
    .btn.primary { background: var(--primary); color: #fff; box-shadow: 0 12px 30px rgba(31, 122, 252, 0.25); }
    .btn.primary:hover { transform: translateY(-1px); }
    .btn.danger { background: var(--danger); color: #fff; box-shadow: 0 12px 30px rgba(239, 68, 68, 0.25); }
    .btn.danger:hover { transform: translateY(-1px); }

    .status-pill { display: inline-flex; align-items: center; gap: 6px; padding: 6px 14px; border-radius: 999px; font-size: 14px; font-weight: 600; margin-top: 12px; }
    .status-pill.success { background: #d1fae5; color: #15803d; }
    .status-pill.error { background: #fee2e2; color: #b91c1c; }
    .status-pill.warning { background: #fef3c7; color: #92400e; }

    @media (max-width: 900px) {
      .hero-content { flex-direction: column; }
      .hero-actions { width: 100%; }
      .btn-link { flex: 1 1 auto; justify-content: center; }
    }

    @media (max-width: 768px) {
      body { padding: 18px; }
      .card { padding: 20px; }
      .hero-text h1 { font-size: 26px; }
      .button-row { flex-direction: column; }
      .btn { width: 100%; }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="card hero-card">
      <div class="hero-content">
        <div class="hero-text">
          <p class="eyebrow">SignalK Configuration</p>
          <h1>WiFi AP Settings</h1>
          <p class="subtitle">Configure Access Point credentials and manage WiFi connections.</p>
          <div class="hero-actions">
            <a href="/" class="btn-link primary">Dashboard</a>
            <a href="/admin" class="btn-link secondary">Admin Panel</a>
            <a href="/config" class="btn-link secondary">Network</a>
            <a href="/hardware-settings" class="btn-link secondary">Hardware</a>
            <a href="/ap-settings" class="btn-link secondary">WiFi AP</a>
            <a href="/settings" class="btn-link secondary">Settings</a>
          </div>
        </div>
      </div>
    </div>

    <div class="card subtle-card">
      Current AP SSID: <strong id="current-ssid">Loading...</strong><br>
      To change WiFi AP credentials, use the Reset WiFi button below. The device will restart and create a configuration portal.
    </div>
    <div class="card">
      <div class="card-header">
        <div>
          <h2>WiFi Configuration</h2>
          <p class="muted">Update Access Point SSID and password, or reset WiFi settings.</p>
        </div>
      </div>
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
        <div class="button-row">
          <button type="submit" class="btn primary">Save AP Settings</button>
          <button type="button" class="btn danger" onclick="resetWiFi()">Reset WiFi & Restart</button>
        </div>
        <div id="status-display"></div>
      </form>
    </div>
  </div>
  <script>
    async function load() {
      try {
        const response = await fetch('/api/settings/ap');
        const data = await response.json();
        document.getElementById('current-ssid').textContent = data.ssid || 'ESP32-SignalK';
      } catch (err) {
        document.getElementById('current-ssid').textContent = 'Unknown';
      }
    }

    async function resetWiFi() {
      if (!confirm('This will erase WiFi credentials and restart the device. The AP configuration portal will start. Continue?')) return;

      const statusDisplay = document.getElementById('status-display');
      try {
        const response = await fetch('/api/wifi/reset', { method: 'POST' });
        if (response.ok) {
          statusDisplay.innerHTML = '<span class="status-pill warning">WiFi reset! Device restarting...</span>';
          setTimeout(() => window.location.href = '/', 5000);
        } else {
          const data = await response.json();
          statusDisplay.innerHTML = '<span class="status-pill error">' + (data.error || 'Failed to reset WiFi') + '</span>';
        }
      } catch (err) {
        statusDisplay.innerHTML = '<span class="status-pill error">Network error</span>';
      }
    }

    document.getElementById('apForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      const statusDisplay = document.getElementById('status-display');

      const ssid = document.getElementById('ap_ssid').value;
      const password = document.getElementById('ap_password').value;
      const confirmPassword = document.getElementById('ap_password_confirm').value;

      if (password && password !== confirmPassword) {
        statusDisplay.innerHTML = '<span class="status-pill error">Passwords do not match</span>';
        return;
      }

      if (password && password.length < 8) {
        statusDisplay.innerHTML = '<span class="status-pill error">Password must be at least 8 characters</span>';
        return;
      }

      if (!ssid && !password) {
        statusDisplay.innerHTML = '<span class="status-pill warning">Please enter SSID and/or password</span>';
        return;
      }

      try {
        const response = await fetch('/api/settings/ap', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ ssid: ssid || null, password: password || null })
        });

        if (response.ok) {
          statusDisplay.innerHTML = '<span class="status-pill warning">AP settings saved! RESTART REQUIRED.</span>';
        } else {
          const data = await response.json();
          statusDisplay.innerHTML = '<span class="status-pill error">' + (data.error || 'Failed to save') + '</span>';
        }
      } catch (err) {
        statusDisplay.innerHTML = '<span class="status-pill error">Network error</span>';
      }

      setTimeout(() => { statusDisplay.innerHTML = ''; }, 5000);
    });

    load();
  </script>
</body>
</html>
)rawliteral";

#endif
