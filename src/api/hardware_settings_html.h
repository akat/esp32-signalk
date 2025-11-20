#ifndef HARDWARE_SETTINGS_HTML_H
#define HARDWARE_SETTINGS_HTML_H

const char HARDWARE_SETTINGS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Hardware Settings - SignalK</title>
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
    .container { max-width: 900px; margin: 0 auto; }

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

    .section { margin-bottom: 24px; }
    .section h3 { font-size: 16px; font-weight: 600; color: var(--text); margin-bottom: 16px; padding-bottom: 8px; border-bottom: 2px solid var(--border); }
    .form-group { margin-bottom: 18px; }
    .form-row { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; margin-bottom: 18px; }
    label { display: block; color: var(--text); margin-bottom: 8px; font-weight: 600; font-size: 14px; }
    input[type="number"] { width: 100%; padding: 12px; border: 1px solid var(--border); border-radius: 12px; font-size: 15px; background: #f9fafb; transition: border 0.2s ease; }
    input[type="number"]:focus { border-color: var(--primary); outline: none; background: #fff; }

    .btn { border: none; border-radius: 12px; padding: 12px 18px; font-size: 15px; font-weight: 600; cursor: pointer; transition: transform 0.2s ease, box-shadow 0.2s ease; }
    .btn.primary { background: var(--primary); color: #fff; box-shadow: 0 12px 30px rgba(31, 122, 252, 0.25); }
    .btn.primary:hover { transform: translateY(-1px); }

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
      .form-row { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="card hero-card">
      <div class="hero-content">
        <div class="hero-text">
          <p class="eyebrow">SignalK Configuration</p>
          <h1>Hardware Settings</h1>
          <p class="subtitle">Configure GPIO pins and baud rates for GPS, RS485, Seatalk1, and CAN bus interfaces.</p>
        </div>
        <div class="hero-actions">
          <a href="/" class="btn-link primary">Dashboard</a>
          <a href="/admin" class="btn-link secondary">Admin Panel</a>
          <a href="/config" class="btn-link secondary">Network</a>
          <a href="/settings" class="btn-link secondary">Password</a>
          <a href="/ap-settings" class="btn-link secondary">WiFi AP</a>
        </div>
      </div>
    </div>
    <div class="card">
      <div class="card-header">
        <div>
          <h2>Hardware Configuration</h2>
          <p class="muted">Configure GPIO pins and communication parameters for all interfaces.</p>
        </div>
      </div>
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
          <div class="form-row">
            <div class="form-group"><label>RX Pin</label><input type="number" id="seatalk1_rx" required min="0" max="39"></div>
            <div class="form-group"><label>Baud Rate</label><input type="number" id="seatalk1_baud" required value="4800"></div>
          </div>
        </div>
        <div class="section">
          <h3>CAN Bus</h3>
          <div class="form-row">
            <div class="form-group"><label>RX Pin</label><input type="number" id="can_rx" required min="0" max="39"></div>
            <div class="form-group"><label>TX Pin</label><input type="number" id="can_tx" required min="0" max="39"></div>
          </div>
        </div>
        <button type="submit" class="btn primary">Save Hardware Settings</button>
        <div id="status-display"></div>
      </form>
    </div>
  </div>
  <script>
    async function load() {
      try {
        const response = await fetch('/api/settings/hardware');
        const data = await response.json();
        document.getElementById('gps_rx').value = data.gps.rx;
        document.getElementById('gps_tx').value = data.gps.tx;
        document.getElementById('gps_baud').value = data.gps.baud;
        document.getElementById('rs485_rx').value = data.rs485.rx;
        document.getElementById('rs485_tx').value = data.rs485.tx;
        document.getElementById('rs485_de').value = data.rs485.de;
        document.getElementById('rs485_de_enable').value = data.rs485.de_enable;
        document.getElementById('rs485_baud').value = data.rs485.baud;
        document.getElementById('seatalk1_rx').value = data.seatalk1.rx;
        document.getElementById('seatalk1_baud').value = data.seatalk1.baud;
        document.getElementById('can_rx').value = data.can.rx;
        document.getElementById('can_tx').value = data.can.tx;
      } catch (err) {
        console.error('Failed to load settings:', err);
      }
    }

    document.getElementById('hwForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      const statusDisplay = document.getElementById('status-display');

      const config = {
        gps: {
          rx: parseInt(document.getElementById('gps_rx').value),
          tx: parseInt(document.getElementById('gps_tx').value),
          baud: parseInt(document.getElementById('gps_baud').value)
        },
        rs485: {
          rx: parseInt(document.getElementById('rs485_rx').value),
          tx: parseInt(document.getElementById('rs485_tx').value),
          de: parseInt(document.getElementById('rs485_de').value),
          de_enable: parseInt(document.getElementById('rs485_de_enable').value),
          baud: parseInt(document.getElementById('rs485_baud').value)
        },
        seatalk1: {
          rx: parseInt(document.getElementById('seatalk1_rx').value),
          baud: parseInt(document.getElementById('seatalk1_baud').value)
        },
        can: {
          rx: parseInt(document.getElementById('can_rx').value),
          tx: parseInt(document.getElementById('can_tx').value)
        }
      };

      try {
        const response = await fetch('/api/settings/hardware', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(config)
        });

        if (response.ok) {
          statusDisplay.innerHTML = '<span class="status-pill warning">Settings saved! Restart required.</span>';
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
