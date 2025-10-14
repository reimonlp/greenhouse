// Dashboard: serve from LittleFS when available; optional minimal fallback
#include "dashboard.h"
#include "sensors.h"
#include "relays.h"
#include "system.h"
#include <LittleFS.h>

DashboardManager dashboard;

DashboardManager::DashboardManager() { server = nullptr; }
DashboardManager::~DashboardManager() {}

bool DashboardManager::begin() {
    if (!LittleFS.begin(true)) {
#ifndef FEATURE_NO_DASHBOARD_FALLBACK
        DEBUG_PRINTLN(F("LittleFS mount failed - using minimal embedded UI"));
#else
        DEBUG_PRINTLN(F("LittleFS mount failed - no dashboard fallback"));
#endif
    } else {
        DEBUG_PRINTLN(F("LittleFS mounted OK"));
    }
    return true;
}

void DashboardManager::setupRoutes(AsyncWebServer* webServer) {
    server = webServer;

        // Security headers - Relaxed CSP for better compatibility
    DefaultHeaders::Instance().addHeader("X-Frame-Options", "DENY");
    DefaultHeaders::Instance().addHeader("X-Content-Type-Options", "nosniff");
    DefaultHeaders::Instance().addHeader("Content-Security-Policy", "default-src 'self'; connect-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; object-src 'none'; frame-ancestors 'none'; base-uri 'self';");

    // Serve LittleFS root; index.html served explicitly (no gzip variant handling).
    server->on("/", HTTP_ANY, [this](AsyncWebServerRequest* req){
        // Allow cache bust query (?v=) by ignoring query portion when serving index
        serveStaticCompressed(req, "/index.html", "text/html", true);
    });
    server->on("/index.html", HTTP_ANY, [this](AsyncWebServerRequest* req){
        serveStaticCompressed(req, "/index.html", "text/html", true);
    });

    // Static asset routes with proper MIME types
    server->on("/style.css", HTTP_ANY, [this](AsyncWebServerRequest* req){
        serveStaticCompressed(req, "/style.css", "text/css", false);
    });
    server->on("/script.js", HTTP_ANY, [this](AsyncWebServerRequest* req){
        serveStaticCompressed(req, "/script.js", "application/javascript", false);
    });
    // Endpoint de diagnóstico para listar archivos del FS
    server->on("/fslist", HTTP_GET, [](AsyncWebServerRequest* req){
        if (!LittleFS.begin()) { // no format
            req->send(500, "application/json", "{\"error\":\"LittleFS mount failed\"}");
            return;
        }
        File root = LittleFS.open("/");
        if (!root) { req->send(500, "application/json", "{\"error\":\"open root failed\"}"); return; }
        String out = "[";
        bool first = true;
        File f = root.openNextFile();
        while (f) {
            if (!first) out += ','; else first = false;
            out += "{\"name\":\"" + String(f.name()) + "\",\"size\":" + String((unsigned)f.size()) + "}";
            f = root.openNextFile();
        }
        out += "]";
        req->send(200, "application/json", out);
    });

    // Fallback SPA route aliases
    server->on("/dashboard", HTTP_ANY, [this](AsyncWebServerRequest* req){
        serveStaticCompressed(req, "/index.html", "text/html", true);
    });

    // WebSocket endpoint for live data
    ws = new AsyncWebSocket("/ws");
    ws->onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        this->onWebSocketEvent(server, client, type, arg, data, len);
    });
    server->addHandler(ws);

    // Favicon route
    server->on("/favicon.svg", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (LittleFS.exists("/favicon.svg")) {
            req->send(LittleFS, "/favicon.svg", "image/svg+xml");
        } else {
            req->send(404, "text/plain", "Not found");
        }
    });
    server->on("/favicon.ico", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (LittleFS.exists("/favicon.svg")) {
            req->send(LittleFS, "/favicon.svg", "image/svg+xml");
        } else {
            // Generate simple SVG favicon
            req->send(200, "image/svg+xml", 
                "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'>"
                "<rect width='32' height='32' fill='#2d5a27'/>"
                "<text x='16' y='24' text-anchor='middle' font-size='20' fill='#4caf50'>🌱</text>"
                "</svg>");
        }
    });

    // Simple 404 for static assets missing
    server->onNotFound([](AsyncWebServerRequest* req){
        if (LittleFS.exists("/404.html")) {
            req->send(LittleFS, "/404.html", "text/html");
        } else {
            req->send(404, "text/plain", "Not found");
        }
    });
}

void DashboardManager::serveStaticCompressed(AsyncWebServerRequest* req, const char* path, const char* contentType, bool allowFallback) {
    bool isIndex = strcmp(path, "/index.html") == 0;
    // 1) Try plain file
    if (LittleFS.exists(path)) {
        AsyncWebServerResponse* resp = req->beginResponse(LittleFS, path, contentType);
        resp->addHeader("Cache-Control", isIndex ? "no-cache, no-store, must-revalidate" : "public, max-age=300");
        req->send(resp);
        return;
    }
    // 2) Try gzip variant ("/file.ext.gz") if present
    char gzPath[64];
    size_t plen = strlen(path);
    if (plen < (sizeof(gzPath) - 4)) { // room for .gz
        strcpy(gzPath, path);
        strcat(gzPath, ".gz");
        if (LittleFS.exists(gzPath)) {
            AsyncWebServerResponse* resp = req->beginResponse(LittleFS, gzPath, contentType);
            resp->addHeader("Content-Encoding", "gzip");
            resp->addHeader("Cache-Control", isIndex ? "no-cache, no-store, must-revalidate" : "public, max-age=600");
            req->send(resp);
            return;
        }
    }
    // 3) No custom JS fallback: if /script.js missing we return 404 (explicit policy)
    if (strcmp(path, "/script.js") == 0) {
        // Do not silently mask the absence; ensures deployment issues surface clearly.
    }
    if (allowFallback) {
#ifndef FEATURE_NO_DASHBOARD_FALLBACK
        // Provide fallback generated content depending on file
    if (strcmp(path, "/index.html") == 0) { req->send(200, "text/html", getIndexHTML()); return; }
#else
        req->send(404, "text/plain", "asset missing");
        return;
#endif
    }
    req->send(404, "text/plain", "Not found");
}

void DashboardManager::sendSensorUpdate(const SensorData& data){
    if(!ws) return;
    StaticJsonDocument<640> doc;
    doc["type"] = "sensor_data";
    doc["timestamp"] = millis();
    JsonObject payload = doc.createNestedObject("data");
    payload["temperature"] = data.temperature;
    payload["humidity"] = data.humidity;
    payload["soil_moisture_1"] = data.soil_moisture_1;
    payload["soil_moisture_2"] = data.soil_moisture_2;
    payload["temp_sensor_1"] = data.temp_sensor_1;
    payload["temp_sensor_2"] = data.temp_sensor_2;
    payload["valid"] = data.valid;
    
    // Add sensor flags for better status indication
    JsonObject flags = payload.createNestedObject("flags");
    flags["dht"] = (data.temperature > -40 && data.humidity >= 0); // Basic DHT validity check
    flags["soil_complete"] = (data.soil_moisture_1 >= 0 && data.soil_moisture_2 >= 0);
    flags["ext_temps_complete"] = (data.temp_sensor_1 > -50 && data.temp_sensor_2 > -50);
    flags["overall_complete"] = flags["dht"] && flags["soil_complete"] && flags["ext_temps_complete"];
    
    String out; 
    serializeJson(doc, out);
    ws->textAll(out);
}

void DashboardManager::sendSystemHeartbeat(){
    if(!ws) return;
    StaticJsonDocument<256> doc;
    doc["type"] = "system_heartbeat";
    doc["timestamp"] = millis();
    JsonObject payload = doc.createNestedObject("data");
    payload["free_heap"] = ESP.getFreeHeap();
    payload["uptime"] = millis();
    payload["clients"] = ws->count();
    String out; 
    serializeJson(doc, out);
    ws->textAll(out);
}

// Fallback completamente deshabilitado (FEATURE_NO_DASHBOARD_FALLBACK definido)

String DashboardManager::formatUptime(unsigned long uptime) {
    unsigned long seconds = uptime / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    char buf[32];
    if (hours > 0) {
        snprintf(buf, sizeof(buf), "%luh %lum", hours, minutes % 60);
    } else {
        snprintf(buf, sizeof(buf), "%lum %lus", minutes, seconds % 60);
    }
    return String(buf);
}

// WebSocket Event Handler
void DashboardManager::onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch(type) {
        case WS_EVT_CONNECT:
            DEBUG_PRINTF("[WS] Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            // Send welcome message with current status
            {
                StaticJsonDocument<256> doc;
                doc["type"] = "welcome";
                doc["timestamp"] = millis();
                doc["client_id"] = client->id();
                JsonObject payload = doc.createNestedObject("data");
                payload["message"] = "Connected to Greenhouse Control System v3.0";
                payload["server_time"] = millis();
                String welcome;
                serializeJson(doc, welcome);
                client->text(welcome);
            }
            break;
            
        case WS_EVT_DISCONNECT:
            DEBUG_PRINTF("[WS] Client #%u disconnected\n", client->id());
            break;
            
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
            
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

// Handle incoming WebSocket messages
void DashboardManager::handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0; // Null terminate string
        
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, (char*)data);
        
        if (error) {
            DEBUG_PRINTF("[WS] JSON parse error: %s\n", error.c_str());
            return;
        }
        
        const char* type = doc["type"];
        if (!type) return;
        
        DEBUG_PRINTF("[WS] Received message type: %s\n", type);
        
        // Handle different message types
        if (strcmp(type, "ping") == 0) {
            // Respond with pong
            StaticJsonDocument<128> response;
            response["type"] = "pong";
            response["timestamp"] = millis();
            String pongMsg;
            serializeJson(response, pongMsg);
            ws->textAll(pongMsg);
        }
        else if (strcmp(type, "request_status") == 0) {
            // Send current system status
            sendSystemHeartbeat();
        }
        // Add more message types as needed
    }
}

// Broadcast message to all connected clients
void DashboardManager::broadcastMessage(const String& type, const String& data) {
    if (!ws) return;
    
    StaticJsonDocument<512> doc;
    doc["type"] = type;
    doc["timestamp"] = millis();
    doc["data"] = data;
    
    String message;
    serializeJson(doc, message);
    ws->textAll(message);
}