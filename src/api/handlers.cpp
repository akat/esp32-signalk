#include "handlers.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Preferences.h>
#include "../types.h"
#include "../services/storage.h"
#include "../services/dyndns.h"
#include "security.h"

// ====== FORWARD DECLARATIONS FOR GLOBALS ======
extern AsyncWebServer server;
extern String vesselUUID;
extern String serverName;
extern std::map<String, PathValue> dataStore;
extern std::map<String, AccessRequestData> accessRequests;
extern std::map<String, ApprovedToken> approvedTokens;
extern std::vector<String> expoTokens;
extern String tcpServerHost;
extern int tcpServerPort;
extern bool tcpEnabled;
extern DynDnsConfig dynDnsConfig;
extern WiFiClient tcpClient;
extern std::map<String, PathValue> lastSentValues;
extern std::set<String> notifications;
extern struct GeofenceConfig geofence;
extern struct DepthAlarmConfig depthAlarm;
extern struct WindAlarmConfig windAlarm;
extern Preferences prefs;

// Forward declarations for utility functions
extern String generateUUID();
extern void setPathValue(const String& path, double value, const String& source, const String& units, const String& description);
extern void setPathValue(const String& path, const String& value, const String& source, const String& units, const String& description);
extern void setPathValueJson(const String& path, const String& jsonStr, const String& source, const String& units, const String& description);
extern void saveApprovedTokens();
extern bool addExpoToken(const String& token);
extern void requestDynDnsUpdate();

// ====== WEB UI CONSTANTS ======
const char* HTML_UI = R"html(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>SignalK Server</title>
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
    code { background: #f0f4ff; padding: 6px 10px; border-radius: 8px; font-family: SFMono-Regular, Consolas, "Liberation Mono", monospace; font-size: 13px; display: inline-block; width: 100%; word-break: break-all; }

    .status-row { display: flex; flex-direction: column; gap: 16px; }
    .label { text-transform: uppercase; font-size: 11px; letter-spacing: 0.08em; color: var(--muted); margin-bottom: 4px; }
    .status-pill { display: inline-flex; align-items: center; gap: 6px; padding: 6px 14px; border-radius: 999px; font-size: 14px; font-weight: 600; }
    .status-pill.connected { background: #d1fae5; color: #15803d; }
    .status-pill.disconnected { background: #fee2e2; color: #b91c1c; }

    .summary-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); gap: 16px; }
    .summary-item { background: #f8f9ff; border: 1px solid #e0e7ff; border-radius: 14px; padding: 14px; }

    .card-header { display: flex; align-items: flex-start; justify-content: space-between; gap: 12px; margin-bottom: 12px; flex-wrap: wrap; }
    .table-wrapper { width: 100%; overflow-x: auto; border: 1px solid var(--border); border-radius: 14px; }
    table { width: 100%; border-collapse: collapse; min-width: 520px; }
    th, td { text-align: left; padding: 14px 16px; border-bottom: 1px solid var(--border); font-size: 14px; vertical-align: top; }
    th { text-transform: uppercase; font-size: 12px; letter-spacing: 0.05em; color: var(--muted); background: #f8fafc; }
    th.path-col, td.path-col { width: 50%; }
    .value { font-weight: 600; color: var(--primary); }
    .timestamp { color: #98a2b3; font-size: 12px; }

    @media (max-width: 900px) {
      .hero-content { flex-direction: column; }
      .hero-actions { width: 100%; }
      .btn-link { flex: 1 1 auto; justify-content: center; }
    }

    @media (max-width: 768px) {
      body { padding: 18px; }
      .card { padding: 20px; }
      .hero-text h1 { font-size: 26px; }
    }

    @media (max-width: 640px) {
      table, thead, tbody, th, td, tr { display: block; width: 100%; }
      thead { display: none; }
      .table-wrapper { border: none; }
      tr { background: #fff; border: 1px solid var(--border); border-radius: 14px; margin-bottom: 14px; padding: 12px; box-shadow: 0 10px 30px rgba(15, 23, 42, 0.05); }
      td { border: none; padding: 8px 0; display: flex; justify-content: space-between; gap: 12px; font-size: 13px; }
      td::before { content: attr(data-label); font-weight: 600; color: var(--muted); text-transform: uppercase; font-size: 11px; letter-spacing: 0.05em; }
      td.path-col { flex-direction: column; }
      td.path-col code { margin-top: 4px; }
      td:last-child { border-bottom: none; }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="card hero-card">
      <div class="hero-content">
        <div class="hero-text">
          <p class="eyebrow">SignalK Server</p>
          <h1>Vessel Dashboard</h1>
          <p class="subtitle">Monitor live SignalK data from your ESP32 gateway and confirm connections at a glance.</p>
        </div>
        <div class="hero-actions">
          <a href="/config" class="btn-link primary">TCP Settings</a>
          <a href="/admin" class="btn-link secondary">Admin Panel</a>
        </div>
      </div>
    </div>

    <div class="card">
      <div class="status-row">
        <div>
          <p class="label">WebSocket Status</p>
          <span id="ws-status" class="status-pill disconnected">Disconnected</span>
        </div>
        <div class="summary-grid">
          <div class="summary-item">
            <p class="label">Server URL</p>
            <code id="server-url"></code>
          </div>
          <div class="summary-item">
            <p class="label">Vessel UUID</p>
            <code id="vessel-uuid"></code>
          </div>
        </div>
      </div>
    </div>

    <div class="card">
      <div class="card-header">
        <div>
          <h2>Navigation Data</h2>
          <p class="muted">Live SignalK deltas streamed over WebSocket.</p>
        </div>
      </div>
      <div class="table-wrapper">
        <table id="data-table">
          <thead>
            <tr>
              <th class="path-col">Path</th>
              <th>Value</th>
              <th>Units</th>
              <th>Timestamp</th>
            </tr>
          </thead>
          <tbody></tbody>
        </table>
      </div>
    </div>
  </div>

  <script>
    const wsUrl = (location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/signalk/v1/stream';
    const ws = new WebSocket(wsUrl);
    const statusEl = document.getElementById('ws-status');
    const tbody = document.querySelector('#data-table tbody');
    const paths = new Map();

    document.getElementById('server-url').textContent = location.origin + '/signalk/v1/api/';

    ws.onopen = () => {
      statusEl.textContent = 'Connected';
      statusEl.className = 'status-pill connected';

      ws.send(JSON.stringify({
        context: '*',
        subscribe: [{ path: '*', period: 1000, format: 'delta', policy: 'instant' }]
      }));
    };

    ws.onclose = () => {
      statusEl.textContent = 'Disconnected';
      statusEl.className = 'status-pill disconnected';
    };

    ws.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);

        if (msg.self) {
          document.getElementById('vessel-uuid').textContent = msg.self;
        }

        if (msg.updates) {
          msg.updates.forEach(update => {
            if (update.values) {
              update.values.forEach(item => {
                const path = item.path;
                let value = item.value;

                if (typeof value === 'number') {
                  value = Number.isInteger(value) ? value : value.toFixed(6);
                } else if (typeof value === 'object') {
                  value = JSON.stringify(value);
                }

                paths.set(path, {
                  value: value,
                  timestamp: update.timestamp || '',
                  units: item.units || '',
                  description: item.description || ''
                });
              });
            }
          });

          renderTable();
        }
      } catch (e) {
        console.error('Error parsing message:', e);
      }
    };

    function renderTable() {
      tbody.innerHTML = '';

      const sortedPaths = Array.from(paths.entries()).sort((a, b) => a[0].localeCompare(b[0]));

      sortedPaths.forEach(([path, data]) => {
        const tr = document.createElement('tr');

        const pathTd = document.createElement('td');
        pathTd.className = 'path-col';
        pathTd.dataset.label = 'Path';
        pathTd.innerHTML = `<code>${path}</code>`;
        if (data.description) {
          pathTd.title = data.description;
        }

        const valueTd = document.createElement('td');
        valueTd.dataset.label = 'Value';
        valueTd.innerHTML = `<span class="value">${data.value}</span>`;

        const unitsTd = document.createElement('td');
        unitsTd.dataset.label = 'Units';
        unitsTd.textContent = data.units || '--';

        const tsTd = document.createElement('td');
        tsTd.dataset.label = 'Timestamp';
        tsTd.innerHTML = `<span class="timestamp">${data.timestamp}</span>`;

        tr.appendChild(pathTd);
        tr.appendChild(valueTd);
        tr.appendChild(unitsTd);
        tr.appendChild(tsTd);

        tbody.appendChild(tr);
      });
    }
  </script>
</body>
</html>
)html";

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
    select { width: 100%; padding: 12px; border: 1px solid var(--border); border-radius: 12px; background: #f9fafb; font-size: 15px; }

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
    .card-header { display: flex; align-items: center; justify-content: space-between; gap: 12px; flex-wrap: wrap; margin-bottom: 12px; }

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

    <div class="card">
      <div class="card-header">
        <div>
          <h2>Dynamic DNS (DynDNS)</h2>
          <p class="muted">Keep your hostname in sync when your IP address changes.</p>
        </div>
        <span id="dyndns-status" class="status-pill disconnected">Disabled</span>
      </div>
      <form id="dyndns-form" class="form-grid">
        <div class="input-field">
          <label for="dyndns-provider">Provider</label>
          <select id="dyndns-provider">
            <option value="dyndns">DynDNS (members.dyndns.org)</option>
            <option value="duckdns">DuckDNS</option>
          </select>
        </div>
        <div class="input-field">
          <label for="dyndns-hostname">Hostname</label>
          <input type="text" id="dyndns-hostname" placeholder="yourhost.dyndns.org">
        </div>

        <div class="input-field provider-dyndns">
          <label for="dyndns-username">Username</label>
          <input type="text" id="dyndns-username" placeholder="DynDNS username">
        </div>

        <div class="input-field provider-dyndns">
          <label for="dyndns-password">Password</label>
          <input type="password" id="dyndns-password" placeholder="DynDNS password">
        </div>

        <div class="input-field provider-duckdns" style="display:none;">
          <label for="dyndns-token">DuckDNS Token</label>
          <input type="text" id="dyndns-token" placeholder="DuckDNS token">
        </div>

        <div class="checkbox-field">
          <input type="checkbox" id="dyndns-enabled">
          <div>
            <strong>Enable DynDNS updates</strong>
            <p class="muted" style="margin-top:4px;">Updates run every 15 minutes or whenever you press Update Now.</p>
          </div>
        </div>

        <div class="button-row">
          <button type="submit" class="btn primary">Save DynDNS</button>
          <button type="button" class="btn secondary" id="dyndns-update-now">Update Now</button>
        </div>
        <div id="dyndns-status-display"></div>
      </form>
      <div class="summary-grid">
        <div class="summary-item">
          <p class="label">Last Result</p>
          <strong id="dyndns-last-result">-</strong>
        </div>
        <div class="summary-item">
          <p class="label">Last Update</p>
          <strong id="dyndns-last-updated">-</strong>
        </div>
      </div>
    </div>
  </div>

  <script>
    async function loadTcpConfig() {
      try {
        const data = await (await fetch('/api/tcp/config')).json();
        document.getElementById('host').value = data.host || '';
        document.getElementById('port').value = data.port || 10110;
        document.getElementById('enabled').checked = data.enabled || false;

        document.getElementById('current-host').textContent = data.host || '(not set)';
        document.getElementById('current-port').textContent = data.port || 10110;
        document.getElementById('current-enabled').textContent = data.enabled ? 'Yes' : 'No';
      } catch (err) {
        console.error('Failed to load TCP config:', err);
      }
    }

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

    function updateDynDnsStatus(data) {
      const pill = document.getElementById('dyndns-status');
      if (data.enabled) {
        pill.textContent = 'Enabled';
        pill.className = 'status-pill connected';
      } else {
        pill.textContent = 'Disabled';
        pill.className = 'status-pill disconnected';
      }

      document.getElementById('dyndns-last-result').textContent = data.lastResult || 'No updates yet';
      document.getElementById('dyndns-last-updated').textContent = data.lastUpdated || '-';
    }

    function updateProviderVisibility(provider) {
      const dynFields = document.querySelectorAll('.provider-dyndns');
      const duckFields = document.querySelectorAll('.provider-duckdns');
      dynFields.forEach(el => el.style.display = provider === 'dyndns' ? 'block' : 'none');
      duckFields.forEach(el => el.style.display = provider === 'duckdns' ? 'block' : 'none');
    }

    document.getElementById('dyndns-provider').addEventListener('change', (e) => {
      updateProviderVisibility(e.target.value);
    });

    async function loadDynDnsConfig() {
      try {
        const data = await (await fetch('/api/dyndns/config')).json();
        document.getElementById('dyndns-provider').value = data.provider || 'dyndns';
        updateProviderVisibility(document.getElementById('dyndns-provider').value);
        document.getElementById('dyndns-hostname').value = data.hostname || '';
        document.getElementById('dyndns-username').value = data.username || '';
        document.getElementById('dyndns-password').value = data.password || '';
        document.getElementById('dyndns-token').value = data.token || '';
        document.getElementById('dyndns-enabled').checked = data.enabled || false;
        updateDynDnsStatus(data);
      } catch (err) {
        console.error('Failed to load DynDNS config:', err);
      }
    }

    document.getElementById('dyndns-form').addEventListener('submit', async (e) => {
      e.preventDefault();
      const payload = {
        hostname: document.getElementById('dyndns-hostname').value,
        provider: document.getElementById('dyndns-provider').value,
        username: document.getElementById('dyndns-username').value,
        password: document.getElementById('dyndns-password').value,
        token: document.getElementById('dyndns-token').value,
        enabled: document.getElementById('dyndns-enabled').checked
      };

      const statusDisplay = document.getElementById('dyndns-status-display');

      try {
        const response = await fetch('/api/dyndns/config', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });

        if (response.ok) {
          statusDisplay.innerHTML = '<span class="status-pill connected">DynDNS settings saved</span>';
          loadDynDnsConfig();
        } else {
          statusDisplay.innerHTML = '<span class="status-pill disconnected">Failed to save DynDNS settings</span>';
        }
      } catch (err) {
        statusDisplay.innerHTML = '<span class="status-pill disconnected">Error: ' + err.message + '</span>';
      }

      setTimeout(() => { statusDisplay.innerHTML = ''; }, 4000);
    });

    document.getElementById('dyndns-update-now').addEventListener('click', async () => {
      const statusDisplay = document.getElementById('dyndns-status-display');
      statusDisplay.innerHTML = '<span class="status-pill connected">Requesting update...</span>';
      try {
        await fetch('/api/dyndns/update', { method: 'POST' });
        setTimeout(loadDynDnsConfig, 1500);
      } catch (err) {
        statusDisplay.innerHTML = '<span class="status-pill disconnected">Update failed: ' + err.message + '</span>';
      }
      setTimeout(() => { statusDisplay.innerHTML = ''; }, 4000);
    });

    loadTcpConfig();
    loadDynDnsConfig();
  </script>
</body>
</html>
)html";

const char* HTML_ADMIN = R"html(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Token Management - SignalK</title>
  <style>
    :root {
      --bg: #f5f7fb;
      --card-bg: #ffffff;
      --primary: #1f7afc;
      --text: #1f2a37;
      --muted: #5d6b82;
      --border: #e4e7ec;
    }

    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: var(--bg); color: var(--text); padding: 24px; line-height: 1.45; }
    .container { max-width: 1000px; margin: 0 auto; }

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

    .card-header { display: flex; align-items: center; justify-content: space-between; gap: 12px; flex-wrap: wrap; margin-bottom: 12px; }
    .card-header span { color: var(--muted); font-size: 13px; }

    .table-wrapper { width: 100%; overflow-x: auto; border: 1px solid var(--border); border-radius: 14px; }
    table { width: 100%; border-collapse: collapse; min-width: 520px; }
    th, td { text-align: left; padding: 14px 16px; border-bottom: 1px solid var(--border); font-size: 14px; vertical-align: top; }
    th { text-transform: uppercase; font-size: 12px; letter-spacing: 0.05em; color: var(--muted); background: #f8fafc; }

    .btn { border: none; border-radius: 10px; padding: 8px 14px; font-size: 13px; font-weight: 600; cursor: pointer; transition: opacity 0.2s ease; color: #fff; }
    .btn-approve { background: #22c55e; }
    .btn-deny { background: #ef4444; }
    .btn-revoke { background: #f97316; }
    .btn:hover { opacity: 0.9; }

    .empty-state { text-align: center; padding: 28px; border: 1px dashed #d5d9eb; border-radius: 14px; color: var(--muted); background: #fff; }

    @media (max-width: 900px) {
      .hero-content { flex-direction: column; }
      .hero-actions { width: 100%; }
      .btn-link { flex: 1 1 auto; justify-content: center; }
    }

    @media (max-width: 720px) {
      body { padding: 18px; }
      .card { padding: 20px; }
      .hero-text h1 { font-size: 26px; }
      .table-wrapper { border: none; }
      table, thead, tbody, th, td, tr { display: block; width: 100%; }
      thead { display: none; }
      tr { background: #fff; border: 1px solid var(--border); border-radius: 14px; margin-bottom: 14px; padding: 12px; box-shadow: 0 8px 24px rgba(15, 23, 42, 0.06); }
      td { border: none; padding: 8px 0; display: flex; justify-content: space-between; gap: 12px; font-size: 13px; }
      td::before { content: attr(data-label); font-weight: 600; color: var(--muted); text-transform: uppercase; font-size: 11px; letter-spacing: 0.05em; }
      td:last-child { border-bottom: none; }
      .btn { width: 100%; justify-content: center; }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="card hero-card">
      <div class="hero-content">
        <div class="hero-text">
          <p class="eyebrow">SignalK Admin</p>
          <h1>Token Management</h1>
          <p class="subtitle">Approve, deny, or revoke SignalK access tokens for connected devices.</p>
        </div>
        <div class="hero-actions">
          <a href="/" class="btn-link primary">Dashboard</a>
          <a href="/config" class="btn-link secondary">TCP Settings</a>
        </div>
      </div>
    </div>

    <div class="card">
      <div class="card-header">
        <div>
          <h2>Pending Requests</h2>
          <p class="muted">Authorize new devices requesting access.</p>
        </div>
        <span>Auto-refreshes every 5 seconds</span>
      </div>
      <div id="pending-requests"></div>
    </div>

    <div class="card">
      <div class="card-header">
        <div>
          <h2>Approved Tokens</h2>
          <p class="muted">Manage existing application tokens.</p>
        </div>
      </div>
      <div id="approved-tokens"></div>
    </div>
  </div>

  <script>
    function wrapTable(content) {
      return `<div class="table-wrapper"><table>${content}</table></div>`;
    }

    function renderEmpty(message) {
      return `<div class="empty-state">${message}</div>`;
    }

    function loadData() {
      fetch('/api/admin/tokens')
        .then(r => r.json())
        .then(data => {
          renderPending(data.pending);
          renderApproved(data.approved);
        });
    }

    function renderPending(requests) {
      const el = document.getElementById('pending-requests');
      if (!requests || requests.length === 0) {
        el.innerHTML = renderEmpty('No pending requests');
        return;
      }

      const rows = requests.map(r => `
        <tr>
          <td data-label="Client ID"><strong>${r.clientId}</strong></td>
          <td data-label="Description">${r.description}</td>
          <td data-label="Permissions">${r.permissions}</td>
          <td data-label="Actions">
            <button class="btn btn-approve" onclick="approve('${r.requestId}')">Approve</button>
            <button class="btn btn-deny" onclick="deny('${r.requestId}')">Deny</button>
          </td>
        </tr>
      `).join('');

      el.innerHTML = wrapTable(`<thead><tr><th>Client ID</th><th>Description</th><th>Permissions</th><th>Actions</th></tr></thead><tbody>${rows}</tbody>`);
    }

    function renderApproved(tokens) {
      const el = document.getElementById('approved-tokens');
      if (!tokens || tokens.length === 0) {
        el.innerHTML = renderEmpty('No approved tokens');
        return;
      }

      const rows = tokens.map(t => `
        <tr>
          <td data-label="Client ID"><strong>${t.clientId}</strong></td>
          <td data-label="Description">${t.description}</td>
          <td data-label="Token"><code>${t.token}</code></td>
          <td data-label="Actions">
            <button class="btn btn-revoke" onclick="revoke('${t.token}')">Revoke</button>
          </td>
        </tr>
      `).join('');

      el.innerHTML = wrapTable(`<thead><tr><th>Client ID</th><th>Description</th><th>Token</th><th>Actions</th></tr></thead><tbody>${rows}</tbody>`);
    }

    function approve(requestId) {
      if (!confirm('Approve this access request?')) return;
      fetch(`/api/admin/approve/${requestId}`, {method: 'POST'})
        .then(r => r.json())
        .then(() => { alert('Approved!'); loadData(); });
    }

    function deny(requestId) {
      if (!confirm('Deny this access request?')) return;
      fetch(`/api/admin/deny/${requestId}`, {method: 'POST'})
        .then(r => r.json())
        .then(() => { alert('Denied!'); loadData(); });
    }

    function revoke(token) {
      if (!confirm('Revoke this token? The client will lose access.')) return;
      fetch(`/api/admin/revoke/${token}`, {method: 'POST'})
        .then(r => r.json())
        .then(() => { alert('Token revoked!'); loadData(); });
    }

    loadData();
    setInterval(loadData, 5000);
  </script>
</body>
</html>
)html";

// ====== SIGNALK API HANDLERS ======

void handleSignalKRoot(AsyncWebServerRequest* req) {
  Serial.println("\n=== /signalk DISCOVERY REQUEST ===");
  Serial.printf("Client IP: %s\n", req->client()->remoteIP().toString().c_str());

  DynamicJsonDocument doc(1024);

  JsonObject endpoints = doc.createNestedObject("endpoints");
  JsonObject v1 = endpoints.createNestedObject("v1");

  v1["version"] = "1.7.0";
  v1["signalk-http"] = "http://" + WiFi.localIP().toString() + ":3000/signalk/v1/api/";
  v1["signalk-ws"] = "ws://" + WiFi.localIP().toString() + ":3000/signalk/v1/stream";

  JsonObject server = doc.createNestedObject("server");
  server["id"] = serverName;
  server["version"] = "1.0.0";

  String output;
  serializeJson(doc, output);

  Serial.println("Response:");
  Serial.println(output);
  Serial.println("==================================\n");

  req->send(200, "application/json", output);
}

void handleAPIRoot(AsyncWebServerRequest* req) {
  DynamicJsonDocument doc(512);
  doc["name"] = serverName;
  doc["version"] = "1.7.0";
  doc["self"] = "vessels." + vesselUUID;

  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
}

void handleAccessRequest(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  if (index + len != total) {
    return;
  }

  Serial.println("\n=== ACCESS REQUEST (POST) ===");
  Serial.printf("Body: %.*s\n", len, data);

  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, data, len);

  String clientId = doc["clientId"] | "unknown";
  String description = doc["description"] | "SensESP client";
  String permissions = doc["permissions"] | "readwrite";

  Serial.printf("Client ID: %s\n", clientId.c_str());
  Serial.printf("Description: %s\n", description.c_str());
  Serial.printf("Permissions: %s\n", permissions.c_str());

  String requestId = clientId;

  for (auto& pair : approvedTokens) {
    if (pair.second.clientId == clientId) {
      Serial.printf("Client already has approved token: %s\n", pair.second.token.c_str());

      DynamicJsonDocument response(512);
      response["state"] = "COMPLETED";
      response["statusCode"] = 200;
      response["requestId"] = requestId;

      JsonObject accessRequest = response.createNestedObject("accessRequest");
      accessRequest["permission"] = "APPROVED";
      accessRequest["token"] = pair.second.token;

      String output;
      serializeJson(response, output);
      req->send(202, "application/json", output);
      Serial.println("=================================\n");
      return;
    }
  }

  AccessRequestData reqData;
  reqData.requestId = requestId;
  reqData.clientId = clientId;
  reqData.description = description;
  reqData.permissions = permissions;
  reqData.token = "";
  reqData.state = "PENDING";
  reqData.permission = "";
  reqData.timestamp = millis();
  accessRequests[requestId] = reqData;

  DynamicJsonDocument response(256);
  response["state"] = "PENDING";
  response["requestId"] = requestId;
  response["href"] = "/signalk/v1/access/requests/" + requestId;

  String output;
  serializeJson(response, output);

  Serial.printf("Response (202 PENDING): %s\n", output.c_str());
  Serial.printf("RequestId: %s - Awaiting manual approval\n", requestId.c_str());
  Serial.println("=================================\n");

  req->send(202, "application/json", output);
}

void handleGetAccessRequestById(AsyncWebServerRequest* req) {
  String uri = req->url();
  Serial.printf("\n=== GET ACCESS REQUEST: %s ===\n", uri.c_str());

  int lastSlash = uri.lastIndexOf('/');
  String requestId = uri.substring(lastSlash + 1);

  Serial.printf("RequestId: %s\n", requestId.c_str());

  // Check if we already have an approved token for this clientId
  for (auto& pair : approvedTokens) {
    if (pair.second.clientId == requestId) {
      DynamicJsonDocument response(512);
      response["state"] = "COMPLETED";
      response["statusCode"] = 200;
      response["requestId"] = requestId;

      JsonObject accessRequest = response.createNestedObject("accessRequest");
      accessRequest["permission"] = "APPROVED";
      accessRequest["token"] = pair.second.token;

      String output;
      serializeJson(response, output);

      req->send(200, "application/json", output);
      return;
    }
  }

  if (accessRequests.find(requestId) == accessRequests.end()) {
    String newToken = generateUUID();

    ApprovedToken approvedToken;
    approvedToken.token = newToken;
    approvedToken.clientId = requestId;
    approvedToken.description = "SensESP (auto-approved after reboot)";
    approvedToken.permissions = "readwrite";
    approvedToken.approvedAt = millis();
    approvedTokens[newToken] = approvedToken;

    saveApprovedTokens();

    DynamicJsonDocument response(512);
    response["state"] = "COMPLETED";
    response["statusCode"] = 200;
    response["requestId"] = requestId;

    JsonObject accessRequest = response.createNestedObject("accessRequest");
    accessRequest["permission"] = "APPROVED";
    accessRequest["token"] = newToken;

    String output;
    serializeJson(response, output);

    req->send(200, "application/json", output);
    return;
  }

  AccessRequestData& reqData = accessRequests[requestId];

  DynamicJsonDocument response(512);
  response["state"] = reqData.state;
  response["requestId"] = requestId;

  if (reqData.state == "COMPLETED") {
    response["statusCode"] = 200;
    JsonObject accessRequest = response.createNestedObject("accessRequest");
    accessRequest["permission"] = reqData.permission;

    if (reqData.permission == "APPROVED") {
      accessRequest["token"] = reqData.token;
    }

    Serial.printf("Response: %s with token: %s\n", reqData.permission.c_str(), reqData.token.c_str());
  } else {
    response["href"] = "/signalk/v1/access/requests/" + requestId;
    Serial.println("Response: Still PENDING");
  }

  String output;
  serializeJson(response, output);

  Serial.printf("Response JSON: %s\n", output.c_str());
  Serial.println("==============================\n");

  req->send(200, "application/json", output);
}

void handleGetAccessRequests(AsyncWebServerRequest* req) {
  String url = req->url();

  if (url.length() > 28 && url.startsWith("/signalk/v1/access/requests/")) {
    Serial.println("=== Detected requestId in URL, routing to handleGetAccessRequestById ===");
    Serial.printf("URL: %s\n", url.c_str());
    handleGetAccessRequestById(req);
    return;
  }

  Serial.println("=== GET /signalk/v1/access/requests - Returning empty list ===");
  req->send(200, "application/json", "[]");
}

void handleAuthValidate(AsyncWebServerRequest* req) {
  Serial.println("\n=== AUTH VALIDATION REQUEST ===");
  Serial.printf("Client IP: %s\n", req->client()->remoteIP().toString().c_str());

  if (req->hasHeader("Authorization")) {
    String auth = req->header("Authorization");
    Serial.printf("Authorization header: %s\n", auth.c_str());
  } else {
    Serial.println("No Authorization header present");
  }

  DynamicJsonDocument doc(256);
  doc["valid"] = true;
  doc["state"] = "COMPLETED";
  doc["statusCode"] = 200;

  String output;
  serializeJson(doc, output);
  Serial.printf("Response: %s\n", output.c_str());
  Serial.println("===============================\n");

  req->send(200, "application/json", output);
}

void handleVesselsSelf(AsyncWebServerRequest* req) {
  Serial.println("\n=== GET /signalk/v1/api/vessels/self ===");
  Serial.printf("dataStore has %d items\n", dataStore.size());

  DynamicJsonDocument doc(4096);

  doc["uuid"] = vesselUUID;
  doc["name"] = serverName;

  if (dataStore.size() > 0) {
    JsonObject nav = doc.createNestedObject("navigation");

    for (auto& kv : dataStore) {
      String path = kv.first;
      if (!path.startsWith("navigation.")) continue;
      Serial.printf("Processing navigation path: %s\n", path.c_str());

      String subPath = path.substring(11);

      JsonObject current = nav;

      int dotIndex = 0;
      int prevDotIndex = 0;
      while ((dotIndex = subPath.indexOf('.', prevDotIndex)) >= 0) {
        String part = subPath.substring(prevDotIndex, dotIndex);
        if (!current.containsKey(part)) {
          current = current.createNestedObject(part);
        } else {
          JsonVariant v = current[part];
          if (v.is<JsonObject>()) {
            current = v.as<JsonObject>();
          } else {
            current = current.createNestedObject(part);
          }
        }
        prevDotIndex = dotIndex + 1;
      }

      String finalKey = subPath.substring(prevDotIndex);
      subPath = finalKey;

      JsonObject value = current.createNestedObject(subPath);
      value["timestamp"] = kv.second.timestamp;

      if (kv.second.isJson) {
        DynamicJsonDocument valueDoc(512);
        DeserializationError err = deserializeJson(valueDoc, kv.second.jsonValue);
        if (!err) {
          value["value"] = valueDoc.as<JsonVariant>();
        } else {
          value["value"] = kv.second.jsonValue;
        }
      } else if (kv.second.isNumeric) {
        value["value"] = kv.second.numValue;
      } else {
        value["value"] = kv.second.strValue;
      }

      JsonObject meta = value.createNestedObject("meta");
      if (kv.second.units.length() > 0) meta["units"] = kv.second.units;
      if (kv.second.description.length() > 0) meta["description"] = kv.second.description;

      JsonObject src = value.createNestedObject("$source");
      src["label"] = kv.second.source;
    }
  }

  JsonObject env = doc.createNestedObject("environment");
  for (auto& kv : dataStore) {
    String path = kv.first;
    if (!path.startsWith("environment.")) continue;

    String subPath = path.substring(12);

    JsonObject current = env;

    int dotIndex = 0;
    int prevDotIndex = 0;
    while ((dotIndex = subPath.indexOf('.', prevDotIndex)) >= 0) {
      String part = subPath.substring(prevDotIndex, dotIndex);
      if (!current.containsKey(part)) {
        current = current.createNestedObject(part);
      } else {
        JsonVariant v = current[part];
        if (v.is<JsonObject>()) {
          current = v.as<JsonObject>();
        } else {
          current = current.createNestedObject(part);
        }
      }
      prevDotIndex = dotIndex + 1;
    }

    String finalKey = subPath.substring(prevDotIndex);
    subPath = finalKey;

    JsonObject value = current.createNestedObject(subPath);
    value["timestamp"] = kv.second.timestamp;

    if (kv.second.isJson) {
      DynamicJsonDocument valueDoc(512);
      DeserializationError err = deserializeJson(valueDoc, kv.second.jsonValue);
      if (!err) {
        value["value"] = valueDoc.as<JsonVariant>();
      } else {
        value["value"] = kv.second.jsonValue;
      }
    } else if (kv.second.isNumeric) {
      value["value"] = kv.second.numValue;
    } else {
      value["value"] = kv.second.strValue;
    }

    JsonObject meta = value.createNestedObject("meta");
    if (kv.second.units.length() > 0) meta["units"] = kv.second.units;
    if (kv.second.description.length() > 0) meta["description"] = kv.second.description;

    JsonObject src = value.createNestedObject("$source");
    src["label"] = kv.second.source;
  }

  if (!notifications.empty()) {
    JsonObject notifs = doc.createNestedObject("notifications");
    for (auto& kv : dataStore) {
      String path = kv.first;
      if (!path.startsWith("notifications.")) continue;

      String subPath = path.substring(14);

      if (kv.second.isJson) {
        DynamicJsonDocument valueDoc(512);
        DeserializationError err = deserializeJson(valueDoc, kv.second.jsonValue);
        if (!err) {
          notifs[subPath] = valueDoc.as<JsonVariant>();
        }
      }
    }
  }

  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
}

void handleGetPath(AsyncWebServerRequest* req) {
  String path = req->url().substring(String("/signalk/v1/api/vessels/self/").length());
  path.replace("/", ".");

  if (dataStore.count(path) == 0) {
    req->send(404, "application/json", "{\"error\":\"Path not found\"}");
    return;
  }

  PathValue& pv = dataStore[path];
  DynamicJsonDocument doc(512);

  if (pv.isJson) {
    DynamicJsonDocument valueDoc(256);
    DeserializationError err = deserializeJson(valueDoc, pv.jsonValue);
    if (!err) {
      doc["value"] = valueDoc.as<JsonVariant>();
    } else {
      doc["value"] = pv.jsonValue;
    }
  } else if (pv.isNumeric) {
    doc["value"] = pv.numValue;
  } else {
    doc["value"] = pv.strValue;
  }

  doc["timestamp"] = pv.timestamp;
  doc["$source"] = pv.source;

  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
}

void handlePutPath(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  if (index + len != total) {
    return;
  }

  String path = req->url().substring(String("/signalk/v1/api/vessels/self/").length());
  path.replace("/", ".");

  Serial.println("\n=== PUT PATH REQUEST ===");
  Serial.printf("Full URL: %s\n", req->url().c_str());
  Serial.printf("Path: %s\n", path.c_str());
  Serial.printf("Data length: %d bytes\n", len);
  Serial.printf("Raw body: %.*s\n", len, data);

  String token = extractBearerToken(req);
  if (token.length() > 0) {
    if (!isTokenValid(token)) {
      Serial.printf("Invalid token provided: %s\n", token.c_str());
      req->send(401, "application/json", "{\"state\":\"COMPLETED\",\"statusCode\":401,\"message\":\"Unauthorized - Invalid token\"}");
      return;
    }
    Serial.printf("Valid token: %s\n", token.substring(0, 15).c_str());
  } else {
    Serial.println("No token provided (open access mode)");
  }

  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    Serial.printf("JSON parse error: %s\n", error.c_str());
    req->send(400, "application/json", "{\"state\":\"COMPLETED\",\"statusCode\":400,\"message\":\"Invalid JSON\"}");
    return;
  }

  String debugJson;
  serializeJsonPretty(doc, debugJson);
  Serial.printf("Parsed JSON:\n%s\n", debugJson.c_str());

  JsonVariant value;
  if (doc.containsKey("value")) {
    value = doc["value"];
  } else {
    value = doc.as<JsonVariant>();
  }

  String source = doc["source"] | "app";
  String description = doc["description"] | "Set by client";

  if (value.isNull()) {
    req->send(400, "application/json", "{\"state\":\"COMPLETED\",\"statusCode\":400,\"message\":\"Value cannot be null\"}");
    return;
  } else if (value.is<double>() || value.is<int>() || value.is<float>()) {
    double numValue = value.as<double>();
    setPathValue(path, numValue, source, "", description);
    Serial.printf("Set numeric value: %.6f\n", numValue);
  } else if (value.is<bool>()) {
    double boolValue = value.as<bool>() ? 1.0 : 0.0;
    setPathValue(path, boolValue, source, "", description);
    Serial.printf("Set boolean value: %s\n", value.as<bool>() ? "true" : "false");
  } else if (value.is<const char*>() || value.is<String>()) {
    String strValue = value.as<String>();
    setPathValue(path, strValue, source, "", description);
    Serial.printf("Set string value: %s\n", strValue.c_str());
  } else if (value.is<JsonObject>() || value.is<JsonArray>()) {
    String jsonStr;
    serializeJson(value, jsonStr);
    setPathValueJson(path, jsonStr, source, "", description.length() > 0 ? description : "HTTP PUT update");
    Serial.printf("Set object/array value: %s\n", jsonStr.c_str());
  } else {
    Serial.println("Unsupported value type");
    req->send(400, "application/json", "{\"state\":\"COMPLETED\",\"statusCode\":400,\"message\":\"Unsupported value type\"}");
    return;
  }

  Serial.println("Path updated successfully");
  Serial.println("=======================\n");

  DynamicJsonDocument response(256);
  response["state"] = "COMPLETED";
  response["statusCode"] = 200;

  String output;
  serializeJson(response, output);
  req->send(200, "application/json", output);
}

// ====== WEB UI HANDLERS ======

void handleRoot(AsyncWebServerRequest* req) {
  req->send(200, "text/html", HTML_UI);
}

void handleConfig(AsyncWebServerRequest* req) {
  req->send(200, "text/html", HTML_CONFIG);
}

void handleAdmin(AsyncWebServerRequest* req) {
  req->send(200, "text/html", HTML_ADMIN);
}

// ====== ADMIN API HANDLERS ======

void handleGetAdminTokens(AsyncWebServerRequest* req) {
  DynamicJsonDocument doc(2048);

  JsonArray pending = doc.createNestedArray("pending");
  for (auto& pair : accessRequests) {
    if (pair.second.state == "PENDING") {
      JsonObject obj = pending.createNestedObject();
      obj["requestId"] = pair.second.requestId;
      obj["clientId"] = pair.second.clientId;
      obj["description"] = pair.second.description;
      obj["permissions"] = pair.second.permissions;
    }
  }

  JsonArray approved = doc.createNestedArray("approved");
  for (auto& pair : approvedTokens) {
    JsonObject obj = approved.createNestedObject();
    obj["token"] = pair.second.token;
    obj["clientId"] = pair.second.clientId;
    obj["description"] = pair.second.description;
    obj["permissions"] = pair.second.permissions;
  }

  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
}

void handleAdminApiPost(AsyncWebServerRequest* req) {
  String url = req->url();
  Serial.printf("\n=== ADMIN API POST: %s ===\n", url.c_str());

  if (url.startsWith("/api/admin/approve/")) {
    String requestId = url.substring(19);
    Serial.printf("Routing to APPROVE: %s\n", requestId.c_str());

    if (accessRequests.find(requestId) == accessRequests.end()) {
      req->send(404, "application/json", "{\"error\":\"Request not found\"}");
      return;
    }

    AccessRequestData& reqData = accessRequests[requestId];

    String token = "APPROVED-" + String(esp_random(), HEX);

    reqData.token = token;
    reqData.state = "COMPLETED";
    reqData.permission = "APPROVED";

    ApprovedToken approvedToken;
    approvedToken.token = token;
    approvedToken.clientId = reqData.clientId;
    approvedToken.description = reqData.description;
    approvedToken.permissions = reqData.permissions;
    approvedToken.approvedAt = millis();
    approvedTokens[token] = approvedToken;

    saveApprovedTokens();

    Serial.printf("Token approved: %s\n", token.c_str());
    Serial.printf("Client: %s\n", reqData.clientId.c_str());
    Serial.println("================================\n");

    DynamicJsonDocument response(256);
    response["success"] = true;
    response["token"] = token;

    String output;
    serializeJson(response, output);
    req->send(200, "application/json", output);
    return;
  }

  if (url.startsWith("/api/admin/deny/")) {
    String requestId = url.substring(16);
    Serial.printf("Routing to DENY: %s\n", requestId.c_str());

    if (accessRequests.find(requestId) == accessRequests.end()) {
      req->send(404, "application/json", "{\"error\":\"Request not found\"}");
      return;
    }

    AccessRequestData& reqData = accessRequests[requestId];
    reqData.state = "COMPLETED";
    reqData.permission = "DENIED";

    Serial.printf("Request denied for client: %s\n", reqData.clientId.c_str());
    Serial.println("================================\n");

    req->send(200, "application/json", "{\"success\":true}");
    return;
  }

  if (url.startsWith("/api/admin/revoke/")) {
    String token = url.substring(18);
    Serial.printf("Routing to REVOKE: %s\n", token.c_str());

    if (approvedTokens.find(token) == approvedTokens.end()) {
      req->send(404, "application/json", "{\"error\":\"Token not found\"}");
      return;
    }

    String clientId = approvedTokens[token].clientId;
    approvedTokens.erase(token);

    saveApprovedTokens();

    Serial.printf("Token revoked for client: %s\n", clientId.c_str());
    Serial.println("================================\n");

    req->send(200, "application/json", "{\"success\":true}");
    return;
  }

  req->send(404, "application/json", "{\"error\":\"Unknown admin API route\"}");
}

void handleApproveToken(AsyncWebServerRequest* req) {
  String url = req->url();
  int lastSlash = url.lastIndexOf('/');
  String requestId = url.substring(lastSlash + 1);

  Serial.printf("\n=== APPROVE REQUEST: %s ===\n", requestId.c_str());

  if (accessRequests.find(requestId) == accessRequests.end()) {
    req->send(404, "application/json", "{\"error\":\"Request not found\"}");
    return;
  }

  AccessRequestData& reqData = accessRequests[requestId];

  String token = "APPROVED-" + String(esp_random(), HEX);

  reqData.token = token;
  reqData.state = "COMPLETED";
  reqData.permission = "APPROVED";

  ApprovedToken approvedToken;
  approvedToken.token = token;
  approvedToken.clientId = reqData.clientId;
  approvedToken.description = reqData.description;
  approvedToken.permissions = reqData.permissions;
  approvedToken.approvedAt = millis();
  approvedTokens[token] = approvedToken;

  saveApprovedTokens();

  Serial.printf("Token approved: %s\n", token.c_str());
  Serial.printf("Client: %s\n", reqData.clientId.c_str());
  Serial.println("================================\n");

  DynamicJsonDocument response(256);
  response["success"] = true;
  response["token"] = token;

  String output;
  serializeJson(response, output);
  req->send(200, "application/json", output);
}

void handleDenyToken(AsyncWebServerRequest* req) {
  String url = req->url();
  int lastSlash = url.lastIndexOf('/');
  String requestId = url.substring(lastSlash + 1);

  Serial.printf("\n=== DENY REQUEST: %s ===\n", requestId.c_str());

  if (accessRequests.find(requestId) == accessRequests.end()) {
    req->send(404, "application/json", "{\"error\":\"Request not found\"}");
    return;
  }

  AccessRequestData& reqData = accessRequests[requestId];
  reqData.state = "COMPLETED";
  reqData.permission = "DENIED";

  Serial.printf("Request denied for client: %s\n", reqData.clientId.c_str());
  Serial.println("================================\n");

  req->send(200, "application/json", "{\"success\":true}");
}

void handleRevokeToken(AsyncWebServerRequest* req) {
  String url = req->url();
  int lastSlash = url.lastIndexOf('/');
  String token = url.substring(lastSlash + 1);

  Serial.printf("\n=== REVOKE TOKEN: %s ===\n", token.c_str());

  if (approvedTokens.find(token) == approvedTokens.end()) {
    req->send(404, "application/json", "{\"error\":\"Token not found\"}");
    return;
  }

  String clientId = approvedTokens[token].clientId;
  approvedTokens.erase(token);

  saveApprovedTokens();

  Serial.printf("Token revoked for client: %s\n", clientId.c_str());
  Serial.println("================================\n");

  req->send(200, "application/json", "{\"success\":true}");
}

void handleRegisterExpoToken(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  if (index + len != total) {
    return;
  }

  Serial.println("\n=== REGISTER EXPO TOKEN ===");

  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    Serial.printf("JSON parse error: %s\n", error.c_str());
    req->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  String token = doc["token"] | "";

  if (token.length() == 0) {
    Serial.println("No token provided");
    req->send(400, "application/json", "{\"error\":\"Token required\"}");
    return;
  }

  if (!token.startsWith("ExponentPushToken[")) {
    Serial.println("Invalid token format");
    req->send(400, "application/json", "{\"error\":\"Invalid token format\"}");
    return;
  }

  bool added = addExpoToken(token);

  Serial.printf("Token: %s\n", token.c_str());
  Serial.printf("Status: %s\n", added ? "Added" : "Already exists");
  Serial.println("================================\n");

  DynamicJsonDocument response(128);
  response["success"] = true;
  response["added"] = added;
  response["totalTokens"] = expoTokens.size();

  String output;
  serializeJson(response, output);
  req->send(200, "application/json", output);
}

// ====== TCP CONFIGURATION HANDLERS ======

void handleGetTcpConfig(AsyncWebServerRequest* req) {
  DynamicJsonDocument doc(256);
  doc["host"] = tcpServerHost;
  doc["port"] = tcpServerPort;
  doc["enabled"] = tcpEnabled;

  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
}

void handleSetTcpConfig(AsyncWebServerRequest* req, uint8_t *data, size_t len, size_t index, size_t total) {
  if (index + len != total) {
    return;
  }

  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    req->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  String host = doc["host"] | "";
  int port = doc["port"] | 10110;
  bool enabled = doc["enabled"] | false;

  prefs.begin("signalk", false);
  prefs.putString("tcp_host", host);
  prefs.putInt("tcp_port", port);
  prefs.putBool("tcp_enabled", enabled);
  prefs.end();

  tcpServerHost = host;
  tcpServerPort = port;
  tcpEnabled = enabled;

  Serial.println("\n=== TCP Configuration Saved ===");
  Serial.println("Host: " + host);
  Serial.println("Port: " + String(port));
  Serial.println("Enabled: " + String(enabled ? "Yes" : "No"));
  Serial.println("===============================\n");

  if (tcpClient.connected()) {
    tcpClient.stop();
  }

  req->send(200, "application/json", "{\"success\":true}");
}

// ====== DYNDNS CONFIGURATION HANDLERS ======

void handleGetDynDnsConfig(AsyncWebServerRequest* req) {
  DynamicJsonDocument doc(512);
  doc["provider"] = dynDnsConfig.provider;
  doc["hostname"] = dynDnsConfig.hostname;
  doc["username"] = dynDnsConfig.username;
  doc["password"] = dynDnsConfig.password;
  doc["token"] = dynDnsConfig.token;
  doc["enabled"] = dynDnsConfig.enabled;
  doc["lastResult"] = dynDnsConfig.lastResult;
  doc["lastUpdated"] = dynDnsConfig.lastUpdated;
  doc["lastSuccess"] = dynDnsConfig.lastSuccess;

  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
}

void handleSetDynDnsConfig(AsyncWebServerRequest* req, uint8_t *data, size_t len, size_t index, size_t total) {
  if (index + len != total) {
    return;
  }

  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    req->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  DynDnsConfig newConfig = dynDnsConfig;
  newConfig.provider = doc["provider"] | dynDnsConfig.provider;
  if (newConfig.provider != "duckdns") {
    newConfig.provider = "dyndns";
  }
  newConfig.hostname = doc["hostname"] | "";
  newConfig.username = doc["username"] | "";
  newConfig.password = doc["password"] | "";
  newConfig.token = doc["token"] | "";
  newConfig.enabled = doc["enabled"] | false;

  saveDynDnsConfig(newConfig);
  requestDynDnsUpdate();

  req->send(200, "application/json", "{\"success\":true}");
}

void handleTriggerDynDnsUpdate(AsyncWebServerRequest* req) {
  requestDynDnsUpdate();
  DynamicJsonDocument doc(256);
  doc["success"] = true;
  doc["message"] = "DynDNS update scheduled";

  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
}
