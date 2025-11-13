#include "handlers.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Preferences.h>
#include "../types.h"
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

// ====== WEB UI CONSTANTS ======
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
    <h1>SignalK Server - ESP32</h1>
    <div style="margin-bottom: 20px;">
      <a href="/config">TCP Configuration</a>
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
    <h1>TCP Configuration</h1>
    <div style="margin-bottom: 20px;">
      <a href="/">Back to Main</a>
    </div>

    <div class="info">
      Configure connection to an external SignalK server via TCP. The server will receive and display data from the external source. Typical SignalK TCP port is 10110.
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

const char* HTML_ADMIN = R"html(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Token Management - SignalK</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #f5f5f5; padding: 20px; }
    .container { max-width: 900px; margin: 0 auto; }
    .card { background: white; border-radius: 8px; padding: 20px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    h1 { color: #333; margin-bottom: 10px; font-size: 24px; }
    h2 { color: #666; margin-bottom: 15px; font-size: 18px; }
    table { width: 100%; border-collapse: collapse; }
    th, td { text-align: left; padding: 12px; border-bottom: 1px solid #eee; }
    th { background: #f8f8f8; font-weight: 600; }
    .btn { padding: 8px 16px; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; }
    .btn-approve { background: #4CAF50; color: white; }
    .btn-deny { background: #f44336; color: white; }
    .btn-revoke { background: #ff9800; color: white; }
    .btn:hover { opacity: 0.9; }
    .status { display: inline-block; padding: 4px 8px; border-radius: 4px; font-size: 12px; font-weight: 600; }
    .status-pending { background: #fff3cd; color: #856404; }
    .status-approved { background: #d4edda; color: #155724; }
    .empty { text-align: center; color: #999; padding: 20px; }
  </style>
</head>
<body>
  <div class="container">
    <div class="card">
      <h1>Token Management</h1>
      <p style="color: #666; margin-top: 10px;">Manage access tokens for SignalK clients</p>
    </div>

    <div class="card">
      <h2>Pending Requests</h2>
      <div id="pending-requests"></div>
    </div>

    <div class="card">
      <h2>Approved Tokens</h2>
      <div id="approved-tokens"></div>
    </div>
  </div>

  <script>
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
        el.innerHTML = '<div class="empty">No pending requests</div>';
        return;
      }

      el.innerHTML = '<table><tr><th>Client ID</th><th>Description</th><th>Permissions</th><th>Actions</th></tr>' +
        requests.map(r => `
          <tr>
            <td><strong>${r.clientId}</strong></td>
            <td>${r.description}</td>
            <td>${r.permissions}</td>
            <td>
              <button class="btn btn-approve" onclick="approve('${r.requestId}')">Approve</button>
              <button class="btn btn-deny" onclick="deny('${r.requestId}')">Deny</button>
            </td>
          </tr>
        `).join('') + '</table>';
    }

    function renderApproved(tokens) {
      const el = document.getElementById('approved-tokens');
      if (!tokens || tokens.length === 0) {
        el.innerHTML = '<div class="empty">No approved tokens</div>';
        return;
      }

      el.innerHTML = '<table><tr><th>Client ID</th><th>Description</th><th>Token</th><th>Actions</th></tr>' +
        tokens.map(t => `
          <tr>
            <td><strong>${t.clientId}</strong></td>
            <td>${t.description}</td>
            <td><code style="font-size:11px">${t.token.substring(0,20)}...</code></td>
            <td>
              <button class="btn btn-revoke" onclick="revoke('${t.token}')">Revoke</button>
            </td>
          </tr>
        `).join('') + '</table>';
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

  if (accessRequests.find(requestId) == accessRequests.end()) {
    Serial.println("RequestId not found - Auto-generating approval for SensESP compatibility");

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

    Serial.printf("Auto-approved! Generated token: %s\n", newToken.c_str());
    Serial.printf("Response JSON: %s\n", output.c_str());
    Serial.println("==============================\n");

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
    if (path == "navigation.anchor.akat" && value.is<JsonObject>()) {
      JsonObject obj = value.as<JsonObject>();

      Serial.println("HTTP: === Processing navigation.anchor.akat update ===");

      bool depthAlarmChanged = false;
      bool windAlarmChanged = false;
      bool anchorPosChanged = false;
      bool radiusChanged = false;

      if (obj.containsKey("depth")) {
        JsonObject depth = obj["depth"];
        if (depth.containsKey("alarm")) {
          if (depth["alarm"].as<bool>() != depthAlarm.enabled) {
            depthAlarmChanged = true;
          }
        }
        if (depth.containsKey("min_depth")) {
          if (abs(depth["min_depth"].as<double>() - depthAlarm.threshold) > 0.01) {
            depthAlarmChanged = true;
          }
        }
      }

      if (obj.containsKey("wind")) {
        JsonObject wind = obj["wind"];
        if (wind.containsKey("alarm")) {
          if (wind["alarm"].as<bool>() != windAlarm.enabled) {
            windAlarmChanged = true;
          }
        }
        if (wind.containsKey("max_speed")) {
          if (abs(wind["max_speed"].as<double>() - windAlarm.threshold) > 0.01) {
            windAlarmChanged = true;
          }
        }
      }

      if (obj.containsKey("anchor")) {
        JsonObject anchor = obj["anchor"];

        if (anchor.containsKey("lat") && anchor.containsKey("lon")) {
          double newLat = anchor["lat"];
          double newLon = anchor["lon"];
          if (abs(newLat - geofence.anchorLat) > 0.00001 || abs(newLon - geofence.anchorLon) > 0.00001) {
            anchorPosChanged = true;
          }
          geofence.anchorLat = newLat;
          geofence.anchorLon = newLon;
          geofence.anchorTimestamp = millis();
        }

        if (anchor.containsKey("radius")) {
          double newRadius = anchor["radius"];
          if (abs(newRadius - geofence.radius) > 0.1) {
            radiusChanged = true;
          }
          geofence.radius = newRadius;
        }

        if (anchor.containsKey("enabled")) {
          bool newEnabled = anchor["enabled"];
          bool isGeofenceUpdate = anchorPosChanged || radiusChanged || (!depthAlarmChanged && !windAlarmChanged);

          if (isGeofenceUpdate) {
            if (newEnabled != geofence.enabled) {
              geofence.enabled = newEnabled;
              Serial.printf("HTTP: Geofence enabled CHANGED to %s\n", geofence.enabled ? "true" : "false");
            }
          }
        }
      }

      if (obj.containsKey("depth")) {
        JsonObject depth = obj["depth"];
        if (depth.containsKey("min_depth")) {
          depthAlarm.threshold = depth["min_depth"];
          Serial.printf("HTTP: Depth threshold: %.1f m\n", depthAlarm.threshold);
        }
        if (depth.containsKey("alarm")) {
          depthAlarm.enabled = depth["alarm"];
          Serial.printf("HTTP: Depth alarm: %s\n", depthAlarm.enabled ? "ENABLED" : "DISABLED");
        }
      }

      if (obj.containsKey("wind")) {
        JsonObject wind = obj["wind"];
        if (wind.containsKey("max_speed")) {
          windAlarm.threshold = wind["max_speed"];
          Serial.printf("HTTP: Wind threshold: %.1f kn\n", windAlarm.threshold);
        }
        if (wind.containsKey("alarm")) {
          windAlarm.enabled = wind["alarm"];
          Serial.printf("HTTP: Wind alarm: %s\n", windAlarm.enabled ? "ENABLED" : "DISABLED");
        }
      }

      DynamicJsonDocument correctedDoc(512);
      JsonObject correctedObj = correctedDoc.to<JsonObject>();

      JsonObject anchorObj = correctedObj.createNestedObject("anchor");
      anchorObj["enabled"] = geofence.enabled;
      anchorObj["radius"] = geofence.radius;
      anchorObj["lat"] = geofence.anchorLat;
      anchorObj["lon"] = geofence.anchorLon;

      JsonObject depthObj = correctedObj.createNestedObject("depth");
      depthObj["alarm"] = depthAlarm.enabled;
      depthObj["min_depth"] = depthAlarm.threshold;

      JsonObject windObj = correctedObj.createNestedObject("wind");
      windObj["alarm"] = windAlarm.enabled;
      windObj["max_speed"] = windAlarm.threshold;

      String correctedJson;
      serializeJson(correctedObj, correctedJson);

      Serial.printf("HTTP: Persisting corrected config: %s\n", correctedJson.c_str());
      setPathValueJson("navigation.anchor.akat", correctedJson, "http.put", "", "HTTP PUT update");
    } else {
      String jsonStr;
      serializeJson(value, jsonStr);
      setPathValueJson(path, jsonStr, source, "", description);
      Serial.printf("Set object/array value: %s\n", jsonStr.c_str());
    }
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
