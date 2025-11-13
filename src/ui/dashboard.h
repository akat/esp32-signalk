#ifndef DASHBOARD_HTML_H
#define DASHBOARD_HTML_H

const char* HTML_UI = R"html(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>SignalK Server</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #f5f5f5; padding: 20px; }
    .container { max-width: 1200px; margin: 0 auto; }
    h1 { font-size: 24px; margin-bottom: 20px; color: #333; }
    .card { background: white; border-radius: 8px; padding: 20px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    .status { display: inline-block; padding: 4px 12px; border-radius: 12px; font-size: 14px; font-weight: 500; }
    .status.connected { background: #4caf50; color: white; }
    .status.disconnected { background: #f44336; color: white; }
    table { width: 100%; border-collapse: collapse; margin-top: 10px; }
    th, td { text-align: left; padding: 12px 8px; border-bottom: 1px solid #eee; font-size: 14px; }
    th { font-weight: 600; color: #666; background: #f9f9f9; }
    code { background: #f0f0f0; padding: 2px 6px; border-radius: 3px; font-family: monospace; font-size: 13px; }
    .value { font-weight: 500; color: #2196f3; }
    .timestamp { color: #999; font-size: 12px; }
    a { color: #2196f3; text-decoration: none; font-weight: 500; }
    a:hover { text-decoration: underline; }
  </style>
</head>
<body>
  <div class="container">
    <h1>🚢 SignalK Server - ESP32</h1>
    <div style="margin-bottom: 20px;">
      <a href="/config">⚙️ TCP Configuration</a>
    </div>

    <div class="card">
      <strong>WebSocket:</strong> <span id="ws-status" class="status disconnected">Disconnected</span>
      <br><br>
      <strong>Server:</strong> <code id="server-url"></code><br>
      <strong>Vessel UUID:</strong> <code id="vessel-uuid"></code>
    </div>

    <div class="card">
      <h2 style="font-size: 18px; margin-bottom: 15px;">Navigation Data</h2>
      <table id="data-table">
        <thead>
          <tr>
            <th>Path</th>
            <th>Value</th>
            <th>Units</th>
            <th>Timestamp</th>
          </tr>
        </thead>
        <tbody></tbody>
      </table>
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
      statusEl.className = 'status connected';

      ws.send(JSON.stringify({
        context: '*',
        subscribe: [{ path: '*', period: 1000, format: 'delta', policy: 'instant' }]
      }));
    };

    ws.onclose = () => {
      statusEl.textContent = 'Disconnected';
      statusEl.className = 'status disconnected';
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
                  value = value.toFixed(6);
                } else if (typeof value === 'object') {
                  value = JSON.stringify(value);
                }

                paths.set(path, {
                  value: value,
                  timestamp: update.timestamp || '',
                  units: item.units || ''
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
        pathTd.innerHTML = `<code>${path}</code>`;

        const valueTd = document.createElement('td');
        valueTd.innerHTML = `<span class="value">${data.value}</span>`;

        const unitsTd = document.createElement('td');
        unitsTd.textContent = data.units;

        const tsTd = document.createElement('td');
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
