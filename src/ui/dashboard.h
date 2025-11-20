#ifndef DASHBOARD_HTML_H
#define DASHBOARD_HTML_H

const char* HTML_UI = R"html(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>SignalK Server - TEST VERSION</title>
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
          <a href="/" class="btn-link primary">Dashboard</a>
          <a href="/admin" class="btn-link secondary">Admin Panel</a>
          <a href="/config" class="btn-link secondary">Network</a>
          <a href="/settings" class="btn-link secondary">Password</a>
          <a href="/hardware-settings" class="btn-link secondary">Hardware</a>
          <a href="/ap-settings" class="btn-link secondary">WiFi AP</a>
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

                // Skip items without a valid path
                if (!path || typeof path !== 'string') {
                  console.warn('Skipping item with invalid path:', item);
                  return;
                }

                // Skip items without a value
                if (item.value === undefined || item.value === null) {
                  console.warn('Skipping item with missing value:', item);
                  return;
                }

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

      const sortedPaths = Array.from(paths.entries())
        .filter(([path]) => path && typeof path === 'string')
        .sort((a, b) => a[0].localeCompare(b[0]));

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

#endif // DASHBOARD_HTML_H
