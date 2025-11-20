#include "routes.h"
#include "handlers.h"
#include "web_auth.h"
#include "settings_html.h"
#include <ArduinoJson.h>

extern AsyncWebSocket ws;

void setupRoutes(AsyncWebServer& server) {
  Serial.println("\n=== Setting up HTTP routes ===\n");

  // HTTP GET handler for /signalk/v1/stream - Token validation for SensESP
  // IMPORTANT: This MUST be registered BEFORE the WebSocket handler!
  // SensESP v3.1.1 validates tokens by making GET request to /signalk/v1/stream
  // and expects HTTP 426 "Upgrade Required" to indicate valid token
  server.on("/signalk/v1/stream", HTTP_GET, [](AsyncWebServerRequest* req) {
    Serial.println("\n=== GET /signalk/v1/stream (Token Validation) ===");
    Serial.printf("Client IP: %s\n", req->client()->remoteIP().toString().c_str());

    // Check for Authorization header
    if (req->hasHeader("Authorization")) {
      String auth = req->header("Authorization");
      Serial.printf("Authorization: %s\n", auth.c_str());

      // Extract token from "Bearer <token>" format
      if (auth.startsWith("Bearer ")) {
        String token = auth.substring(7);
        token.trim();
        Serial.printf("Token: %s\n", token.c_str());

        // We'll check token validity via the security module
        // For now, accept all tokens (permissive mode)
        Serial.println("Token found - returning 426");
        req->send(426, "text/plain", "Upgrade Required");
        Serial.println("Response: 426 Upgrade Required");
        Serial.println("======================================\n");
        return;
      }
    }

    Serial.println("No valid Authorization header - returning 401");
    req->send(401, "text/plain", "Unauthorized");
    Serial.println("Response: 401 Unauthorized");
    Serial.println("======================================\n");
  });

  // WebSocket setup - handler already registered in main.cpp
  Serial.println("Setting up WebSocket...");
  server.addHandler(&ws);
  Serial.println("WebSocket setup complete");

  // ===== WEB AUTHENTICATION ROUTES =====

  // Login page
  server.on("/login.html", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Login - ESP32 SignalK</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .login-container {
      background: white;
      border-radius: 10px;
      box-shadow: 0 10px 40px rgba(0, 0, 0, 0.2);
      padding: 40px;
      max-width: 400px;
      width: 100%;
    }
    h1 {
      color: #333;
      margin-bottom: 10px;
      font-size: 28px;
      text-align: center;
    }
    .subtitle {
      color: #666;
      text-align: center;
      margin-bottom: 30px;
      font-size: 14px;
    }
    .form-group {
      margin-bottom: 20px;
    }
    label {
      display: block;
      color: #333;
      margin-bottom: 8px;
      font-weight: 500;
      font-size: 14px;
    }
    input[type="text"],
    input[type="password"] {
      width: 100%;
      padding: 12px 15px;
      border: 2px solid #e0e0e0;
      border-radius: 6px;
      font-size: 16px;
      transition: border-color 0.3s;
    }
    input[type="text"]:focus,
    input[type="password"]:focus {
      outline: none;
      border-color: #667eea;
    }
    button {
      width: 100%;
      padding: 14px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      border: none;
      border-radius: 6px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: transform 0.2s, box-shadow 0.2s;
    }
    button:hover {
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);
    }
    button:active {
      transform: translateY(0);
    }
    .error {
      background: #fee;
      border: 1px solid #fcc;
      color: #c33;
      padding: 12px;
      border-radius: 6px;
      margin-bottom: 20px;
      font-size: 14px;
      display: none;
    }
    .error.show {
      display: block;
    }
    .default-creds {
      margin-top: 20px;
      padding: 15px;
      background: #f5f5f5;
      border-radius: 6px;
      font-size: 13px;
      color: #666;
    }
    .default-creds strong {
      color: #333;
    }
  </style>
</head>
<body>
  <div class="login-container">
    <h1>ESP32 SignalK</h1>
    <p class="subtitle">Marine Data Gateway</p>

    <div id="error" class="error"></div>

    <form id="loginForm">
      <div class="form-group">
        <label for="username">Username</label>
        <input type="text" id="username" name="username" value="admin" required autofocus>
      </div>

      <div class="form-group">
        <label for="password">Password</label>
        <input type="password" id="password" name="password" required>
      </div>

      <button type="submit">Login</button>
    </form>

    <div class="default-creds">
      <strong>Default credentials:</strong><br>
      Username: admin<br>
      Password: 12345<br>
      <em>Change password after first login!</em>
    </div>
  </div>

  <script>
    document.getElementById('loginForm').addEventListener('submit', async (e) => {
      e.preventDefault();

      const username = document.getElementById('username').value;
      const password = document.getElementById('password').value;
      const errorDiv = document.getElementById('error');

      try {
        const response = await fetch('/api/auth/login', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ username, password })
        });

        const data = await response.json();

        if (response.ok) {
          // Set cookie
          document.cookie = `session_id=${data.sessionId}; path=/; max-age=1800`;
          // Redirect to main page
          window.location.href = '/';
        } else {
          errorDiv.textContent = data.error || 'Login failed';
          errorDiv.classList.add('show');
        }
      } catch (err) {
        errorDiv.textContent = 'Network error. Please try again.';
        errorDiv.classList.add('show');
      }
    });
  </script>
</body>
</html>
)HTML");
  });

  // Login API endpoint
  server.on("/api/auth/login", HTTP_POST,
    [](AsyncWebServerRequest* req) {}, NULL,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
      if (index + len == total) {
        String body = "";
        for (size_t i = 0; i < len; i++) {
          body += (char)data[i];
        }

        DynamicJsonDocument doc(256);
        DeserializationError err = deserializeJson(doc, body);

        if (err) {
          req->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
          return;
        }

        String username = doc["username"] | "";
        String password = doc["password"] | "";

        if (validateWebCredentials(username, password)) {
          String sessionId = createWebSession(username);

          String response = "{\"success\":true,\"sessionId\":\"" + sessionId + "\"}";
          req->send(200, "application/json", response);
        } else {
          req->send(401, "application/json", "{\"error\":\"Invalid credentials\"}");
        }
      }
    });

  // Logout API endpoint
  server.on("/api/auth/logout", HTTP_POST, [](AsyncWebServerRequest* req) {
    String sessionId = extractSessionCookie(req);
    if (sessionId.length() > 0) {
      destroyWebSession(sessionId);
    }
    req->send(200, "application/json", "{\"success\":true}");
  });

  // Change password API endpoint
  server.on("/api/auth/change-password", HTTP_POST,
    [](AsyncWebServerRequest* req) {}, NULL,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
      // Require authentication
      String sessionId = extractSessionCookie(req);
      if (!validateWebSession(sessionId)) {
        req->send(401, "application/json", "{\"error\":\"Not authenticated\"}");
        return;
      }

      if (index + len == total) {
        String body = "";
        for (size_t i = 0; i < len; i++) {
          body += (char)data[i];
        }

        DynamicJsonDocument doc(512);
        DeserializationError err = deserializeJson(doc, body);

        if (err) {
          req->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
          return;
        }

        String oldPassword = doc["oldPassword"] | "";
        String newPassword = doc["newPassword"] | "";

        if (oldPassword.length() == 0 || newPassword.length() == 0) {
          req->send(400, "application/json", "{\"error\":\"Missing password fields\"}");
          return;
        }

        if (changeWebPassword(oldPassword, newPassword)) {
          req->send(200, "application/json", "{\"success\":true,\"message\":\"Password changed successfully\"}");
        } else {
          req->send(400, "application/json", "{\"error\":\"Invalid old password or new password too short\"}");
        }
      }
    });

  // ===== WEB UI ROUTES (Protected) =====

  // Root UI
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!requireWebAuth(req)) return;
    handleRoot(req);
  });

  // TCP Configuration page
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!requireWebAuth(req)) return;
    handleConfig(req);
  });

  // Admin token management page
  server.on("/admin", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!requireWebAuth(req)) return;
    handleAdmin(req);
  });

  // Settings page
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!requireWebAuth(req)) return;
    req->send(200, "text/html", SETTINGS_HTML);
  });

  // ===== ADMIN API ENDPOINTS (Protected) =====

  // Admin API endpoints
  server.on("/api/admin/tokens", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!requireWebAuth(req)) return;
    handleGetAdminTokens(req);
  });

  // TCP Configuration API
  server.on("/api/tcp/config", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!requireWebAuth(req)) return;
    handleGetTcpConfig(req);
  });
  server.on("/api/tcp/config", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      if (!requireWebAuth(req)) return;
    }, NULL, handleSetTcpConfig);

  // DynDNS Configuration API
  server.on("/api/dyndns/config", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!requireWebAuth(req)) return;
    handleGetDynDnsConfig(req);
  });
  server.on("/api/dyndns/config", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      if (!requireWebAuth(req)) return;
    }, NULL, handleSetDynDnsConfig);
  server.on("/api/dyndns/update", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!requireWebAuth(req)) return;
    handleTriggerDynDnsUpdate(req);
  });

  // Hardware Settings API
  server.on("/api/settings/hardware", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!requireWebAuth(req)) return;
    handleGetHardwareSettings(req);
  });
  server.on("/api/settings/hardware", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      if (!requireWebAuth(req)) return;
    }, NULL, handleSetHardwareSettings);

  // AP Settings API
  server.on("/api/settings/ap", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!requireWebAuth(req)) return;
    handleGetAPSettings(req);
  });
  server.on("/api/settings/ap", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      if (!requireWebAuth(req)) return;
    }, NULL, handleSetAPSettings);

  // Hardware Settings Page
  server.on("/hardware-settings", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!requireWebAuth(req)) return;
    handleHardwareSettingsPage(req);
  });

  // AP Settings Page
  server.on("/ap-settings", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!requireWebAuth(req)) return;
    handleAPSettingsPage(req);
  });

  // WiFi Reset API
  server.on("/api/wifi/reset", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!requireWebAuth(req)) return;
    handleWiFiReset(req);
  });
  // Expo Push Notification API (NOT protected - external services need access)
  server.on("/plugins/signalk-node-red/redApi/register-expo-token", HTTP_POST,
    [](AsyncWebServerRequest* req) {}, NULL, handleRegisterExpoToken);

  // ===== TEST ENDPOINTS =====

  // Test endpoint to verify server is reachable
  server.on("/test", HTTP_GET, [](AsyncWebServerRequest* req) {
    Serial.println("=== TEST ENDPOINT HIT ===");
    Serial.printf("Client IP: %s\n", req->client()->remoteIP().toString().c_str());
    req->send(200, "text/plain", "ESP32 SignalK Server is running on port 3000!");
  });

  server.on("/test", HTTP_POST, [](AsyncWebServerRequest* req) {
    Serial.println("=== TEST POST ENDPOINT HIT ===");
    Serial.printf("Client IP: %s\n", req->client()->remoteIP().toString().c_str());
    req->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"POST received\"}");
  });

  // ===== SignalK Routes (IMPORTANT: Register most specific routes FIRST!) =====

  // Access request polling by requestId - MUST be before /signalk/v1/access/requests
#ifdef ASYNCWEBSERVER_REGEX
  server.on("^\\/signalk\\/v1\\/access\\/requests\\/(.+)$", HTTP_GET, handleGetAccessRequestById);
#endif

  // Access requests (auto-approve for SensESP compatibility)
  server.on("/signalk/v1/access/requests", HTTP_GET, handleGetAccessRequests);
  server.on("/signalk/v1/access/requests", HTTP_POST,
    [](AsyncWebServerRequest* req) {}, NULL, handleAccessRequest);

  // Token validation (always valid)
  server.on("/signalk/v1/auth/validate", HTTP_GET, handleAuthValidate);

  // API root
  server.on("/signalk/v1/api", HTTP_GET, handleAPIRoot);
  server.on("/signalk/v1/api/", HTTP_GET, handleAPIRoot);

  // SignalK discovery - MUST be registered LAST among /signalk routes
  server.on("/signalk", HTTP_GET, handleSignalKRoot);

  // Vessels
  server.on("/signalk/v1/api/vessels/self", HTTP_GET, handleVesselsSelf);
  server.on("/signalk/v1/api/vessels/self/", HTTP_GET, handleVesselsSelf);

#ifdef ASYNCWEBSERVER_REGEX
  // Dynamic path handlers - using regex patterns (requires heavy <regex> support)
  // Note: ESPAsyncWebServer regex support can be unreliable, so we also use rewrite rules
  server.on("^\\/signalk\\/v1\\/api\\/vessels\\/self\\/(.+)$", HTTP_GET, handleGetPath);
  server.on("^\\/signalk\\/v1\\/api\\/vessels\\/self\\/(.+)$", HTTP_PUT,
    [](AsyncWebServerRequest* req) {}, NULL, handlePutPath);
#else
  Serial.println("Regex routing disabled - relying on onNotFound fallback for vessels/self paths");
#endif

  // CORS headers
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");

  // Handle requests with bodies (POST, PUT)
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    String url = request->url();

    Serial.println("\n=== INCOMING REQUEST (onRequestBody) ===");
    Serial.printf("URL: %s\n", url.c_str());
    Serial.printf("Method: ");
    switch (request->method()) {
      case HTTP_GET: Serial.println("GET"); break;
      case HTTP_POST: Serial.println("POST"); break;
      case HTTP_PUT: Serial.println("PUT"); break;
      case HTTP_DELETE: Serial.println("DELETE"); break;
      case HTTP_OPTIONS: Serial.println("OPTIONS"); break;
      default: Serial.println(request->method()); break;
    }
    Serial.printf("Body chunk: index=%d, len=%d, total=%d\n", index, len, total);
    Serial.println("========================================\n");

    // Handle POST requests to access/requests
    if (request->method() == HTTP_POST && url == "/signalk/v1/access/requests") {
      Serial.println("Calling handleAccessRequest from onRequestBody...");
      handleAccessRequest(request, data, len, index, total);
      Serial.println("handleAccessRequest completed");
    }
    // Handle PUT requests to vessels/self paths
    else if (request->method() == HTTP_PUT && url.startsWith("/signalk/v1/api/vessels/self/") && url.length() > 29) {
      Serial.println("Calling handlePutPath from onRequestBody...");
      handlePutPath(request, data, len, index, total);
      Serial.println("handlePutPath completed");
    }
    // Handle POST to admin endpoints
    else if (request->method() == HTTP_POST && url.startsWith("/api/admin/")) {
      Serial.println("Calling handleAdminApiPost from onRequestBody...");
      handleAdminApiPost(request);
      Serial.println("handleAdminApiPost completed");
    }
  });

  // Catch-all handler - route admin API and other paths if regex didn't match
  server.onNotFound([](AsyncWebServerRequest* req) {
    String url = req->url();

    // Debug: Log what we're checking
    Serial.printf("[DEBUG] onNotFound: method=%d, url=%s\n", req->method(), url.c_str());
    Serial.printf("[DEBUG] HTTP_POST value=%d\n", HTTP_POST);
    Serial.printf("[DEBUG] Checking admin API: POST=%d, starts=%d\n",
                  (req->method() == HTTP_POST), url.startsWith("/api/admin/"));

    // Check if this is an admin API POST request
    // Use integer comparison instead of enum comparison
    if (req->method() == 2 && url.startsWith("/api/admin/")) {
      Serial.println("=== ROUTING ADMIN API POST (onNotFound fallback) ===");
      Serial.printf("URL: %s\n", url.c_str());
      handleAdminApiPost(req);
      return;
    }

    // Check if this is an access request polling path
    if (url.startsWith("/signalk/v1/access/requests/") && url.length() > 28) {
      Serial.println("=== ROUTING ACCESS REQUEST BY ID (onNotFound fallback) ===");
      Serial.printf("URL: %s\n", url.c_str());
      if (req->method() == HTTP_GET) {
        handleGetAccessRequestById(req);
      }
      return;
    }

    // Check if this is a vessels/self GET or PUT path
    if (url.startsWith("/signalk/v1/api/vessels/self/") && url.length() > 29) {
      Serial.println("=== ROUTING VESSELS/SELF PATH (onNotFound fallback) ===");
      Serial.printf("URL: %s\n", url.c_str());
      Serial.printf("Method: %d (GET=%d, PUT=%d)\n", req->method(), HTTP_GET, HTTP_PUT);

      if (req->method() == HTTP_GET) {
        handleGetPath(req);
        return;
      } else if (req->method() == HTTP_PUT) {
        // PUT with body is handled in onRequestBody callback
        // Don't send 404 - body handler will respond after processing body
        Serial.println("PUT request detected - waiting for body handler to process");
        return;
      }
    }

    // Default 404
    Serial.println("=== 404 NOT FOUND ===");
    Serial.printf("No handler found for: %s\n", url.c_str());
    req->send(404, "application/json", "{\"error\":\"Not found\"}");
  });

  Serial.println("=== HTTP routes setup complete ===\n");
}
