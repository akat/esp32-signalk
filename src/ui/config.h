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
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #f5f5f5; padding: 20px; }
    .container { max-width: 600px; margin: 0 auto; }
    h1 { font-size: 24px; margin-bottom: 20px; color: #333; }
    .card { background: white; border-radius: 8px; padding: 20px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    label { display: block; margin-bottom: 8px; font-weight: 500; color: #333; }
    input[type="text"], input[type="number"] { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px; margin-bottom: 16px; }
    .checkbox-wrapper { margin-bottom: 20px; }
    input[type="checkbox"] { width: 20px; height: 20px; margin-right: 8px; cursor: pointer; }
    button { background: #2196f3; color: white; padding: 12px 24px; border: none; border-radius: 4px; font-size: 16px; font-weight: 500; cursor: pointer; width: 100%; margin-bottom: 10px; }
    button:hover { background: #1976d2; }
    button.secondary { background: #757575; }
    button.secondary:hover { background: #616161; }
    .info { background: #e3f2fd; padding: 12px; border-radius: 4px; margin-bottom: 20px; font-size: 14px; color: #1976d2; }
    .status { display: inline-block; padding: 4px 12px; border-radius: 12px; font-size: 14px; font-weight: 500; margin-top: 12px; }
    .status.connected { background: #4caf50; color: white; }
    .status.disconnected { background: #f44336; color: white; }
    a { color: #2196f3; text-decoration: none; }
    a:hover { text-decoration: underline; }
  </style>
</head>
<body>
  <div class="container">
    <h1>⚙️ TCP Configuration</h1>
    <div style="margin-bottom: 20px;">
      <a href="/">← Back to Main</a>
    </div>

    <div class="info">
      Configure connection to an external SignalK server via TCP. The server will receive and display data from the external source. Typical SignalK TCP port is 10110.<br><br>
      <strong>Note:</strong> The ESP32 also runs an NMEA TCP server on port 10110 for receiving mock NMEA data. You can connect to it from another device to send NMEA sentences for testing.
    </div>

    <div class="card">
      <form id="tcp-form">
        <label for="host">Server Hostname or IP:</label>
        <input type="text" id="host" name="host" placeholder="signalk.example.com or 192.168.1.100" required>

        <label for="port">Port:</label>
        <input type="number" id="port" name="port" value="10110" min="1" max="65535" required>

        <div class="checkbox-wrapper">
          <label style="display: inline-flex; align-items: center; cursor: pointer;">
            <input type="checkbox" id="enabled" name="enabled">
            <span>Enable TCP Connection</span>
          </label>
        </div>

        <button type="submit">Save Configuration</button>
        <button type="button" class="secondary" onclick="location.href='/'">Cancel</button>
      </form>

      <div id="status-display"></div>
    </div>

    <div class="card">
      <h2 style="font-size: 18px; margin-bottom: 12px;">Current Configuration</h2>
      <p><strong>Host:</strong> <span id="current-host">-</span></p>
      <p><strong>Port:</strong> <span id="current-port">-</span></p>
      <p><strong>Enabled:</strong> <span id="current-enabled">-</span></p>
    </div>

    <div class="card">
      <h2 style="font-size: 18px; margin-bottom: 12px;">NMEA TCP Server</h2>
      <p style="font-size: 14px; color: #666; line-height: 1.6;">
        The ESP32 runs an NMEA TCP server on port 10110 that accepts connections from NMEA data sources. You can use tools like netcat, telnet, or custom scripts to send NMEA sentences to this port for testing.
      </p>
      <p style="font-size: 14px; color: #666; line-height: 1.6; margin-top: 8px;">
        <strong>Example:</strong> <code>echo '$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A' | nc 192.168.4.1 10110</code>
      </p>
    </div>

    <div class="card">
      <h2 style="font-size: 18px; margin-bottom: 12px;">Troubleshooting</h2>
      <p style="font-size: 14px; color: #666; line-height: 1.6;">
        If the connection fails:
      </p>
      <ul style="font-size: 14px; color: #666; line-height: 1.8; margin-left: 20px; margin-top: 8px;">
        <li>Ensure your ESP32 is connected to the internet via WiFiManager</li>
        <li>Verify the hostname/IP is correct and accessible from your network</li>
        <li>Check that port 10110 (or your configured port) is open on the remote server</li>
        <li>Monitor the serial console for detailed connection logs</li>
        <li>Try using an IP address instead of hostname to rule out DNS issues</li>
      </ul>
    </div>
  </div>

  <script>
    // Load current configuration
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

    // Handle form submission
    document.getElementById('tcp-form').addEventListener('submit', async (e) => {
      e.preventDefault();

      const config = {
        host: document.getElementById('host').value,
        port: parseInt(document.getElementById('port').value),
        enabled: document.getElementById('enabled').checked
      };

      try {
        const response = await fetch('/api/tcp/config', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(config)
        });

        if (response.ok) {
          document.getElementById('status-display').innerHTML =
            '<div class="status connected">Configuration saved successfully!</div>';

          // Update current config display
          document.getElementById('current-host').textContent = config.host || '(not set)';
          document.getElementById('current-port').textContent = config.port;
          document.getElementById('current-enabled').textContent = config.enabled ? 'Yes' : 'No';

          setTimeout(() => {
            document.getElementById('status-display').innerHTML = '';
          }, 3000);
        } else {
          document.getElementById('status-display').innerHTML =
            '<div class="status disconnected">Failed to save configuration</div>';
        }
      } catch (err) {
        document.getElementById('status-display').innerHTML =
          '<div class="status disconnected">Error: ' + err.message + '</div>';
      }
    });
  </script>
</body>
</html>
)html";

#endif // CONFIG_HTML_H
