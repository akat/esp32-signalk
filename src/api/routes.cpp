#include "routes.h"
#include "handlers.h"

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

  // WebSocket setup
  Serial.println("Setting up WebSocket...");
  ws.onEvent(nullptr);  // Event handler is registered in main
  server.addHandler(&ws);
  Serial.println("WebSocket setup complete");

  // ===== WEB UI ROUTES =====

  // Root UI
  server.on("/", HTTP_GET, handleRoot);

  // TCP Configuration page
  server.on("/config", HTTP_GET, handleConfig);

  // Admin token management page
  server.on("/admin", HTTP_GET, handleAdmin);

  // ===== ADMIN API ENDPOINTS =====

  // Admin API endpoints
  server.on("/api/admin/tokens", HTTP_GET, handleGetAdminTokens);

  // TCP Configuration API
  server.on("/api/tcp/config", HTTP_GET, handleGetTcpConfig);
  server.on("/api/tcp/config", HTTP_POST,
    [](AsyncWebServerRequest* req) {}, NULL, handleSetTcpConfig);

  // Expo Push Notification API
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

  Serial.println("=== HTTP routes setup complete ===\n");
}
