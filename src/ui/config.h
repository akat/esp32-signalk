#ifndef CONFIG_HTML_H
#define CONFIG_HTML_H

const char* HTML_CONFIG = R"html(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>TCP Configuration - SignalK</title>
  <style>
    :root {
      --bg: #f5f7fb;
      --card-bg: #ffffff;
      --primary: #1f7afc;
      --primary-dark: #1554c0;
      --text: #1f2a37;
      --muted: #5d6b82;
      --border: #e4e7ec;
    }

    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: var(--bg); color: var(--text); padding: 24px; line-height: 1.45; }
    .container { max-width: 800px; margin: 0 auto; }

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

    .muted { color: var(--muted); font-size: 14px; }
    .subtle-card { background: #eef4ff; border: 1px solid #dbe4ff; color: var(--muted); }

    label { display: block; font-weight: 600; margin-bottom: 8px; color: var(--text); }
    input[type="text"], input[type="number"] { width: 100%; padding: 12px; border: 1px solid var(--border); border-radius: 12px; font-size: 15px; background: #f9fafb; transition: border 0.2s ease; }
    input[type="text"]:focus, input[type="number"]:focus { border-color: var(--primary); outline: none; background: #fff; }

    .form-grid { display: flex; flex-direction: column; gap: 18px; }
    .checkbox-field { display: flex; gap: 12px; align-items: flex-start; }
    .checkbox-field input { width: 20px; height: 20px; margin-top: 4px; }
    .button-row { display: flex; gap: 12px; flex-wrap: wrap; }
    .btn { border: none; border-radius: 12px; padding: 12px 18px; font-size: 15px; font-weight: 600; cursor: pointer; transition: transform 0.2s ease, box-shadow 0.2s ease; }
    .btn.primary { background: var(--primary); color: #fff; box-shadow: 0 12px 30px rgba(31, 122, 252, 0.25); }
    .btn.primary:hover { transform: translateY(-1px); }
    .btn.secondary { background: #e4e7ec; color: #1f2a37; }

    .status-pill { display: inline-flex; align-items: center; gap: 6px; padding: 6px 14px; border-radius: 999px; font-size: 14px; font-weight: 600; }
    .status-pill.connected { background: #d1fae5; color: #15803d; }
    .status-pill.disconnected { background: #fee2e2; color: #b91c1c; }

    .summary-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 16px; }
    .summary-item { background: #f8f9ff; border: 1px solid #e0e7ff; border-radius: 14px; padding: 14px; }
    .label { text-transform: uppercase; font-size: 11px; letter-spacing: 0.08em; color: var(--muted); margin-bottom: 4px; }

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
          <p class="eyebrow">SignalK Server</p>
          <h1>TCP Configuration</h1>
          <p class="subtitle">Link this ESP32 SignalK instance to an upstream server or data bridge.</p>
        </div>
        <div class="hero-actions">
          <a href="/" class="btn-link primary">Dashboard</a>
          <a href="/admin" class="btn-link secondary">Admin Panel</a>
        </div>
      </div>
    </div>

    <div class="card subtle-card">
      Configure a remote SignalK TCP feed. When enabled, this ESP32 will connect to the server and forward incoming data to all local clients.
    </div>

    <div class="card">
      <div class="card-header">
        <div>
          <h2>Remote Server</h2>
          <p class="muted">Set hostname, port, and enable or disable the TCP bridge.</p>
        </div>
      </div>
      <form id="tcp-form" class="form-grid">
        <div class="input-field">
          <label for="host">Server Hostname or IP</label>
          <input type="text" id="host" name="host" placeholder="signalk.example.com or 192.168.1.100" required>
        </div>

        <div class="input-field">
          <label for="port">Port</label>
          <input type="number" id="port" name="port" value="10110" min="1" max="65535" required>
        </div>

        <div class="checkbox-field">
          <input type="checkbox" id="enabled" name="enabled">
          <div>
            <strong>Enable TCP connection</strong>
            <p class="muted" style="margin-top:4px;">When enabled, the ESP32 will automatically maintain the TCP link.</p>
          </div>
        </div>

        <div class="button-row">
          <button type="submit" class="btn primary">Save Configuration</button>
          <button type="button" class="btn secondary" onclick="location.href='/'">Cancel</button>
        </div>
        <div id="status-display"></div>
      </form>
    </div>

    <div class="card">
      <div class="card-header">
        <div>
          <h2>Current Configuration</h2>
          <p class="muted">Snapshot of the saved TCP settings.</p>
        </div>
      </div>
      <div class="summary-grid">
        <div class="summary-item">
          <p class="label">Host</p>
          <strong id="current-host">-</strong>
        </div>
        <div class="summary-item">
          <p class="label">Port</p>
          <strong id="current-port">-</strong>
        </div>
        <div class="summary-item">
          <p class="label">Enabled</p>
          <strong id="current-enabled">-</strong>
        </div>
      </div>
    </div>
  </div>

  <script>
    fetch('/api/tcp/config')
      .then(r => r.json())
      .then(data => {
        document.getElementById('host').value = data.host || '';
        document.getElementById('port').value = data.port || 10110;
        document.getElementById('enabled').checked = data.enabled || false;

        document.getElementById('current-host').textContent = data.host || '(not set)';
        document.getElementById('current-port').textContent = data.port || 10110;
        document.getElementById('current-enabled').textContent = data.enabled ? 'Yes' : 'No';
      })
      .catch(err => console.error('Failed to load config:', err));

    document.getElementById('tcp-form').addEventListener('submit', async (e) => {
      e.preventDefault();

      const config = {
        host: document.getElementById('host').value,
        port: parseInt(document.getElementById('port').value, 10),
        enabled: document.getElementById('enabled').checked
      };

      try {
        const response = await fetch('/api/tcp/config', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(config)
        });

        if (response.ok) {
          document.getElementById('status-display').innerHTML = '<span class="status-pill connected">Configuration saved successfully</span>';

          document.getElementById('current-host').textContent = config.host || '(not set)';
          document.getElementById('current-port').textContent = config.port;
          document.getElementById('current-enabled').textContent = config.enabled ? 'Yes' : 'No';
        } else {
          document.getElementById('status-display').innerHTML = '<span class="status-pill disconnected">Failed to save configuration</span>';
        }
      } catch (err) {
        document.getElementById('status-display').innerHTML = '<span class="status-pill disconnected">Error: ' + err.message + '</span>';
      }

      setTimeout(() => {
        document.getElementById('status-display').innerHTML = '';
      }, 4000);
    });
  </script>
</body>
</html>
)html";

#endif // CONFIG_HTML_H
