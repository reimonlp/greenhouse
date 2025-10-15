#include "api.h"
#include "system.h"
#include "dashboard.h"
#include "database.h"
#include <WiFi.h>
#include "rate_limiter.h"
#include "config_schema.h"
#include "pins.h"
#include <LittleFS.h>
#include "fs_utils.h"
#include "rule_engine.h"

// Global rate limiter instance (window 60s, max requests per config) placed before route lambdas
static RateLimiter<RATE_LIMIT_SLOTS> gRateLimiter(60000UL, MAX_API_REQUESTS);

APIManager api;

APIManager::APIManager() {
    server = nullptr;
    ws = nullptr;
    authToken = API_TOKEN;
    requestCount = 0;
    lastResetTime = 0;
    memset(authTokenHash, 0, sizeof(authTokenHash));
    tokenHashed = false;
    // Rate limiter instance (window 60s, max per config)
    // SLOTS fixed by RATE_LIMIT_SLOTS constant.
}

APIManager::~APIManager() {
    if (ws) delete ws;
    if (server) delete server;
}

bool APIManager::begin() {
    DEBUG_PRINTLN(F("Starting API server..."));
    // Advertencia de seguridad si el token parece un placeholder o demasiado corto
    if (authToken.length() < 12 || authToken == "tu_token_secreto_aqui" || authToken == "REPLACE_ME_TOKEN") {
        DEBUG_PRINTLN(F("[SEC][WARN] API_TOKEN débil o placeholder. Define uno largo en include/secrets.h (>=24 chars aleatorios)."));
    }
    // Intentar cargar hash persistido previamente (rotación pasada)
    bool loaded = loadPersistedTokenHash();
    // Siempre calcular hash del token actual en un buffer temporal para detectar cambios
    uint8_t currentHash[32];
    do {
        if (authToken.isEmpty()) { memset(currentHash,0,sizeof(currentHash)); break; }
#ifdef MBEDTLS_MD_SHA256_C
        #include <mbedtls/sha256.h>
        mbedtls_sha256_context ctx; mbedtls_sha256_init(&ctx); mbedtls_sha256_starts_ret(&ctx, 0);
        mbedtls_sha256_update_ret(&ctx, (const unsigned char*)authToken.c_str(), authToken.length());
        mbedtls_sha256_finish_ret(&ctx, currentHash); mbedtls_sha256_free(&ctx);
#else
        memset(currentHash,0,sizeof(currentHash));
        for (size_t i=0;i<authToken.length(); ++i) currentHash[i%32]^=authToken[i];
#endif
    } while(false);
    if (!loaded) {
        // No había hash previo: persistimos el actual
        memcpy(authTokenHash, currentHash, 32);
        tokenHashed = true;
        if (persistTokenHash()) {
            DEBUG_PRINTLN(F("[SEC] Token hash persisted (initial)"));
        } else {
            DEBUG_PRINTLN(F("[SEC][WARN] Failed to persist token hash (initial)"));
        }
    } else {
        // Hay hash previo: comparar y si difiere, rehash automático
        uint8_t diff=0; for (int i=0;i<32;i++) diff |= (authTokenHash[i]^currentHash[i]);
        if (diff!=0) {
            DEBUG_PRINTLN(F("[SEC] Detected API_TOKEN macro change -> updating stored hash"));
            memcpy(authTokenHash, currentHash, 32);
            tokenHashed = true;
            if (!persistTokenHash()) {
                DEBUG_PRINTLN(F("[SEC][ERROR] Failed to persist updated token hash"));
            }
        } else {
            tokenHashed = true; // Already matches
        }
    }
    
    server = new AsyncWebServer(API_PORT);
    
    // Configurar WebSocket
    setupWebSocket();
    
    // Configurar rutas de la API
    setupRoutes();
    
    // Iniciar servidor
    server->begin();
    
    DEBUG_PRINTLN(String(F("API server started on port ")) + String(API_PORT));
    return true;
}

void APIManager::setupRoutes() {
    // Middleware CORS para todas las rutas
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    
    // Configurar rutas del dashboard
    dashboard.setupRoutes(server);
    
    // ========== ENDPOINTS DE SENSORES ==========
    server->on("/api/sensors", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetSensors(request);
    });
    
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/sensors/history", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetSensorHistory(request);
    });
    #endif
    
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/sensors/calibrate", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleCalibrateSensor(request);
    });
    #endif
    
    // ========== ENDPOINTS DE RELAYS ==========
    server->on("/api/relays", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetRelays(request);
    });
    
    server->on("/api/relays/set", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSetRelay(request);
    });
    
    server->on("/api/relays/mode", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSetRelayMode(request);
    });
    
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/relays/rule", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSetAutoRule(request);
    });
    #endif
    
    // ========== ENDPOINTS DE REGLAS ==========
    #ifndef FEATURE_MINIMAL_API
    // GET /api/relays/X/rules - List all rules for relay
    server->on("/api/relays/0/rules", HTTP_GET, [this](AsyncWebServerRequest *r) { 
        DEBUG_PRINTLN("GET /api/relays/0/rules called");
        handleGetRelayRules(r); 
    });
    server->on("/api/relays/1/rules", HTTP_GET, [this](AsyncWebServerRequest *r) { handleGetRelayRules(r); });
    server->on("/api/relays/2/rules", HTTP_GET, [this](AsyncWebServerRequest *r) { handleGetRelayRules(r); });
    server->on("/api/relays/3/rules", HTTP_GET, [this](AsyncWebServerRequest *r) { handleGetRelayRules(r); });
    
    // POST /api/relays/X/rules - Add new rule (with body handler)
    server->on("/api/relays/0/rules", HTTP_POST, [this](AsyncWebServerRequest *r) { handleAddRelayRule(r); },
               NULL, [this](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total) {
                   appendBodyChunk(r, data, len, index, total);
               });
    server->on("/api/relays/1/rules", HTTP_POST, [this](AsyncWebServerRequest *r) { handleAddRelayRule(r); },
               NULL, [this](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total) {
                   appendBodyChunk(r, data, len, index, total);
               });
    server->on("/api/relays/2/rules", HTTP_POST, [this](AsyncWebServerRequest *r) { handleAddRelayRule(r); },
               NULL, [this](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total) {
                   appendBodyChunk(r, data, len, index, total);
               });
    server->on("/api/relays/3/rules", HTTP_POST, [this](AsyncWebServerRequest *r) { handleAddRelayRule(r); },
               NULL, [this](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total) {
                   appendBodyChunk(r, data, len, index, total);
               });
    
    // DELETE /api/relays/X/rules - Clear all rules
    server->on("/api/relays/0/rules", HTTP_DELETE, [this](AsyncWebServerRequest *r) { handleClearRelayRules(r); });
    server->on("/api/relays/1/rules", HTTP_DELETE, [this](AsyncWebServerRequest *r) { handleClearRelayRules(r); });
    server->on("/api/relays/2/rules", HTTP_DELETE, [this](AsyncWebServerRequest *r) { handleClearRelayRules(r); });
    server->on("/api/relays/3/rules", HTTP_DELETE, [this](AsyncWebServerRequest *r) { handleClearRelayRules(r); });
    
    // PUT/DELETE for individual rules with wildcard (with body handler for PUT)
    server->on("^\\/api\\/relays\\/\\d+\\/rules\\/\\d+$", HTTP_PUT, 
               [this](AsyncWebServerRequest *r) { handleUpdateRelayRule(r); },
               NULL, [this](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total) {
                   appendBodyChunk(r, data, len, index, total);
               });
    server->on("^\\/api\\/relays\\/\\d+\\/rules\\/\\d+$", HTTP_DELETE, [this](AsyncWebServerRequest *r) { handleDeleteRelayRule(r); });
    #endif
    
    // ========== ENDPOINTS DEL SISTEMA ==========
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/system/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetSystemStatus(request);
    });
    #endif

    // Endpoint de features compiladas
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/system/features", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
    StaticJsonDocument<384> doc;
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["build_date"] = BUILD_DATE;
    doc["schema_version"] = CONFIG_SCHEMA_VERSION;
    doc["min_log_level"] = MIN_LOG_LEVEL;
#ifdef FEATURE_DISABLE_OTA
    doc["ota_disabled"] = true;
#else
    doc["ota_disabled"] = false;
#endif
#ifdef FEATURE_DISABLE_REMOTE_DB
    doc["remote_db_disabled"] = true;
#else
    doc["remote_db_disabled"] = false;
#endif
#ifdef FEATURE_NO_DASHBOARD_FALLBACK
    doc["dashboard_fallback_disabled"] = true;
#else
    doc["dashboard_fallback_disabled"] = false;
#endif
#ifdef ENABLE_STATUS_LED
    doc["status_led"] = true;
#else
    doc["status_led"] = false;
#endif
    // Buzzer eliminado: no se incluye flag
    // DHT stabilize param
    doc["dht_stabilize_ms"] = DHT_STABILIZE_MS;
    String out; serializeJson(doc, out);
    sendJSONResponse(request, 200, out);
    });
    #endif

    // Endpoint ligero de salud
    server->on("/api/system/health", HTTP_GET, [this](AsyncWebServerRequest *request) {
        // Public endpoint - no authentication required for health check
        StaticJsonDocument<384> doc;
        doc["uptime"] = system_manager.getUptime();
        doc["free_heap"] = ESP.getFreeHeap();
    doc["min_free_heap"] = system_manager.getMinFreeHeap();
    doc["largest_free_block"] = system_manager.getLargestFreeBlock();
    doc["min_largest_free_block"] = system_manager.getMinLargestFreeBlock();
        doc["fragmentation_ratio"] = system_manager.getFragmentationRatio();
        doc["min_fragmentation_ratio"] = system_manager.getMinFragmentationRatio();
        doc["wifi"] = WiFi.status() == WL_CONNECTED;
        if (WiFi.status() == WL_CONNECTED) doc["rssi"] = WiFi.RSSI();
        doc["state"] = system_manager.getStateString();
        doc["ts"] = system_manager.getCurrentTimestamp();
        doc["loop_avg_us"] = (uint32_t)system_manager.getLoopAvgMicros();
        // Métricas conectividad / NTP reales
        doc["wifi_reconnect_attempts"] = (uint32_t)system_manager.getWifiReconnectAttempts();
        doc["wifi_reconnect_successes"] = (uint32_t)system_manager.getWifiReconnectSuccesses();
        doc["ntp_failures"] = (uint32_t)system_manager.getNtpFailureCount();
        // Last WiFi disconnect reason & small hot reasons subset
        doc["last_wifi_reason"] = system_manager.getLastWifiDisconnectReason();
        JsonObject reasons = doc.createNestedObject("wifi_reasons");
        const uint8_t hotReasons[] = {200,201,202,203,204};
        for (size_t i=0;i<sizeof(hotReasons);++i){
            uint8_t r = hotReasons[i];
            char key[8]; snprintf(key,sizeof(key),"r%u", r);
            reasons[key] = (uint32_t)system_manager.getWifiReasonCount(r);
        }
        String out; serializeJson(doc, out);
        sendJSONResponse(request, 200, out);
    });

    // Endpoint de métricas de reglas automáticas (O2)
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/system/rules", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
        // Reusar getSystemStatus para no duplicar buffers: contiene métricas por regla
        // (eval_total, act_total, eval_hour, act_hour, hour)
        String status = relays.getSystemStatus();
        sendJSONResponse(request, 200, status);
    });
    #endif

    // Endpoint de diagnóstico del rate limiter
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/system/ratelimit", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
        // Capturar snapshot
        extern RateLimiter<RATE_LIMIT_SLOTS> gRateLimiter;
    RateLimiter<RATE_LIMIT_SLOTS>::Snapshot snap;
    gRateLimiter.snapshot(snap);
        StaticJsonDocument<1024> doc; // suficiente para pocas entradas
        doc["window_ms"] = snap.windowMs;
        doc["max_requests"] = snap.maxRequests;
        doc["slots_used"] = snap.active;
        doc["slots_capacity"] = (uint16_t)RATE_LIMIT_SLOTS;
        doc["evictions"] = snap.evictions;
        JsonArray arr = doc.createNestedArray("entries");
        for (uint16_t i=0;i<snap.active && i< (uint16_t)RATE_LIMIT_SLOTS; ++i){
            JsonObject o = arr.createNestedObject();
            IPAddress ip(snap.entries[i].ip);
            char ipbuf[24]; snprintf(ipbuf,sizeof(ipbuf),"%u.%u.%u.%u", ip[0],ip[1],ip[2],ip[3]);
            o["ip"] = ipbuf;
            o["count"] = snap.entries[i].count;
            o["window_start_ms"] = snap.entries[i].windowStart;
        }
        String out; serializeJson(doc,out);
        sendJSONResponse(request, 200, out);
    });
    #endif

    // GPIO diagnostics (basic: only states; potentially heavy if expanded)
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/system/gpio", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
        StaticJsonDocument<512> doc; // keep small
        JsonArray arr = doc.createNestedArray("pins");
    const int pinsToReport[] = { RELAY_LUCES_PIN, RELAY_VENTILADOR_PIN, RELAY_BOMBA_PIN, RELAY_CALEFACTOR_PIN, STATUS_LED_PIN };
        for (size_t i=0;i<sizeof(pinsToReport)/sizeof(pinsToReport[0]); ++i) {
            int p = pinsToReport[i];
            JsonObject o = arr.createNestedObject();
            o["pin"] = p;
            o["level"] = (int)digitalRead(p);
            // Mode introspection not directly available cheaply; annotate known roles
            if (p == RELAY_LUCES_PIN) o["role"] = "relay_luces";
            else if (p == RELAY_VENTILADOR_PIN) o["role"] = "relay_ventilador";
            else if (p == RELAY_BOMBA_PIN) o["role"] = "relay_bomba";
            else if (p == RELAY_CALEFACTOR_PIN) o["role"] = "relay_calefactor";
#ifdef ENABLE_STATUS_LED
            else if (p == STATUS_LED_PIN) o["role"] = "status_led";
#endif
            // Buzzer eliminado
        }
        String out; serializeJson(doc,out);
        sendJSONResponse(request, 200, out);
    });
    #endif

    // Endpoint muy ligero sólo para heap y uptime (O3/A2)
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/system/heap", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
        if (!checkRateLimit(request)) { sendErrorResponse(request, 429, "Rate limit exceeded"); return; }
        char buf[128];
        // Formato JSON manual con métricas de memoria y uptime
        snprintf(buf, sizeof(buf), "{\"free_heap\":%u,\"min_free_heap\":%u,\"largest_free_block\":%u,\"min_largest_free_block\":%u,\"fragmentation_ratio\":%.3f,\"min_fragmentation_ratio\":%.3f,\"uptime\":%lu}",
                 (unsigned)ESP.getFreeHeap(), (unsigned)system_manager.getMinFreeHeap(),
                 (unsigned)system_manager.getLargestFreeBlock(), (unsigned)system_manager.getMinLargestFreeBlock(),
                 system_manager.getFragmentationRatio(), system_manager.getMinFragmentationRatio(),
                 system_manager.getUptime());
        sendJSONResponse(request, 200, String(buf));
    });
    #endif
    
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/system/stats", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetStatistics(request);
    });
    #endif
    
    // Endpoint runtime log level (GET/POST)
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/system/loglevel", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
    StaticJsonDocument<96> doc;
    doc["current"] = database.getRuntimeLogLevel();
    doc["compiled_min"] = MIN_LOG_LEVEL;
        String out; serializeJson(doc, out);
        sendJSONResponse(request, 200, out);
    });
    server->on("/api/system/loglevel", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
        if (!request->hasParam("level", true)) { sendErrorResponse(request, 400, "Missing level param"); return; }
        int level = request->getParam("level", true)->value().toInt();
        if (level < LOG_DEBUG || level > LOG_CRITICAL) { sendErrorResponse(request, 400, "Invalid level"); return; }
        database.setRuntimeLogLevel((LogLevel)level);
    StaticJsonDocument<96> doc;
    doc["success"] = true;
    doc["level"] = level;
        String out; serializeJson(doc, out);
        sendJSONResponse(request, 200, out);
    });
    #endif

    // Endpoint de rotación del token API
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/system/token/rotate", HTTP_POST, [this](AsyncWebServerRequest *request){
        handleRotateToken(request);
    });
    #endif
    
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/system/pause", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSystemPause(request);
    });
    #endif
    
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/system/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSystemReset(request);
    });
    #endif
    
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/system/wifi-reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleWiFiReset(request);
    });
    #endif
    
    // ========== ENDPOINTS DE CONFIGURACIÓN ==========
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetConfig(request);
    });
    
    server->on("/api/config", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSetConfig(request);
    });

    // Backup current (read-only) config plus schema version
    server->on("/api/config/backup", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleBackupConfig(request);
    });

    // Restore config (expects JSON body) - POST /api/config/restore
    server->on("/api/config/restore", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleRestoreConfig(request);
    });
    #endif
    
    // ========== ENDPOINTS DE LOGS ==========
    server->on("/api/logs", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetLogs(request);
    });
    
    // Logs críticos recientes
    server->on("/api/logs/critical", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
        int count = 25;
        if (request->hasParam("count")) {
            int c = request->getParam("count")->value().toInt();
            if (c > 0 && c < 100) count = c; // límite razonable
        }
        String logs = database.getCriticalLogs(count);
        sendJSONResponse(request, 200, logs);
    });
    
    server->on("/api/logs/clear", HTTP_DELETE, [this](AsyncWebServerRequest *request) {
        handleClearLogs(request);
    });
    
    // ========== ENDPOINTS DE OTA ==========
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/firmware/info", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetFirmwareInfo(request);
    });
    #endif
    
    // ========== MANEJO DE OPTIONS (CORS) ==========
    server->onRequestBody([this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        appendBodyChunk(request, data, len, index, total);
    });
    
    // Endpoint de healthz ultra ligero (sin auth) para monitoreo externo
    server->on("/api/healthz", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "ok");
    });
    server->on("/api/healthz", HTTP_HEAD, [this](AsyncWebServerRequest *request){
        request->send(200);
    });

    // Endpoint de uptime aún más ligero: sólo número en ms (sin auth)
    server->on("/api/system/uptime", HTTP_GET, [this](AsyncWebServerRequest *request){
        char buf[24];
        unsigned long up = system_manager.getUptime();
        snprintf(buf, sizeof(buf), "%lu", up);
        request->send(200, "text/plain", buf);
    });

    // Prometheus style metrics (auth optional? keep protected)
    #ifndef FEATURE_MINIMAL_API
    server->on("/metrics", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
        // Build metrics string
        String m;
        m.reserve(1024);
        m += "# HELP system_uptime_milliseconds Uptime since boot in ms\n";
        m += "# TYPE system_uptime_milliseconds counter\n";
        m += "system_uptime_milliseconds "; m += system_manager.getUptime(); m += '\n';
        m += "# HELP system_free_heap_bytes Free heap bytes\n# TYPE system_free_heap_bytes gauge\n";
        m += "system_free_heap_bytes "; m += (unsigned long)ESP.getFreeHeap(); m += '\n';
        m += "# HELP system_min_free_heap_bytes Minimum observed free heap\n# TYPE system_min_free_heap_bytes gauge\n";
        m += "system_min_free_heap_bytes "; m += system_manager.getMinFreeHeap(); m += '\n';
        m += "# HELP system_loop_avg_us Exponential moving average of loop time microseconds\n# TYPE system_loop_avg_us gauge\n";
        m += "system_loop_avg_us "; m += (unsigned long)system_manager.getLoopAvgMicros(); m += '\n';
        m += "# HELP wifi_reconnect_attempts Total WiFi reconnect attempts\n# TYPE wifi_reconnect_attempts counter\n";
        m += "wifi_reconnect_attempts "; m += (unsigned long)system_manager.getWifiReconnectAttempts(); m += '\n';
        m += "# HELP wifi_reconnect_successes Total WiFi reconnect successes\n# TYPE wifi_reconnect_successes counter\n";
        m += "wifi_reconnect_successes "; m += (unsigned long)system_manager.getWifiReconnectSuccesses(); m += '\n';
        m += "# HELP ntp_failures Total NTP failure count\n# TYPE ntp_failures counter\n";
        m += "ntp_failures "; m += (unsigned long)system_manager.getNtpFailureCount(); m += '\n';
        m += "# HELP wifi_last_disconnect_reason Last observed WiFi disconnect reason code\n# TYPE wifi_last_disconnect_reason gauge\n";
        m += "wifi_last_disconnect_reason "; m += (unsigned long)system_manager.getLastWifiDisconnectReason(); m += '\n';
        // Export a small fixed subset of reason counters (prom label form)
        const uint8_t hotReasons[] = {200,201,202,203,204};
        m += "# HELP wifi_disconnect_reasons_total WiFi disconnect counts by reason code\n# TYPE wifi_disconnect_reasons_total counter\n";
        for (size_t i=0;i<sizeof(hotReasons); ++i){
            uint8_t r = hotReasons[i];
            m += "wifi_disconnect_reasons_total{reason=\""; m += r; m += "\"} ";
            m += (unsigned long)system_manager.getWifiReasonCount(r); m += '\n';
        }
        // Rule metrics aggregated
        m += "# HELP rule_evaluations_total Total rule evaluations per relay\n# TYPE rule_evaluations_total counter\n";
        for (int i=0;i<4;++i){
            String ruleJson = relays.getAutoRule(i); // small doc extraction
            // Quick parse for evalCount; cheaper alternative would be direct exposure method
            // For now naive: search substrings (avoid large JSON lib usage).
            int pos = ruleJson.indexOf("\"evalCount\":");
            if (pos>=0){
                int end = ruleJson.indexOf(',', pos+12);
                String num = ruleJson.substring(pos+12, end>0?end:ruleJson.length());
                num.trim();
                m += "rule_evaluations_total{relay=\""; m += i; m += "\"} "; m += num; m += '\n';
            }
        }
        request->send(200, "text/plain; version=0.0.4", m);
    });
    #endif

    // Manejar rutas no encontradas
    server->onNotFound([this](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(200);
        } else {
            sendErrorResponse(request, 404, "Endpoint not found");
        }
    });

    // === Nuevo endpoint: formateo del filesystem (administrativo) ===
    #ifndef FEATURE_MINIMAL_API
    server->on("/api/fs/format", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
        if (!request->hasParam("confirm", true)) { sendErrorResponse(request, 400, "Missing confirm param"); return; }
        String confirm = request->getParam("confirm", true)->value();
        if (confirm != "YES") { sendErrorResponse(request, 400, "Confirm must be YES"); return; }
        bool ok = false;
        if (LittleFS.begin(true)) {
            ok = LittleFS.format();
            LittleFS.begin(true);
        }
        if (ok) {
            database.logSystemEvent("fs_format", "Filesystem formatted via API");
            AsyncResponseStream *res = request->beginResponseStream("application/json");
            res->print('{'); res->print("\"status\":\"ok\",\"message\":\"filesystem formatted, rebooting\"}");
            request->send(res);
            delay(200);
            ESP.restart();
        } else {
            sendErrorResponse(request, 500, "Format failed");
        }
    });
    #endif
}

// ========== HANDLERS DE SENSORES ==========
void APIManager::handleGetSensors(AsyncWebServerRequest* request) {
    // Public endpoint - no authentication required for sensor data
    if (!checkRateLimit(request)) {
        sendErrorResponse(request, 429, "Rate limit exceeded");
        return;
    }
    
    SensorData data = sensors.getCurrentData();
    
    StaticJsonDocument<640> doc;
    doc["timestamp"] = data.timestamp;
    doc["valid"] = data.valid;
    doc["temperature"] = data.temperature;
    doc["humidity"] = data.humidity;
    doc["soil_moisture_1"] = data.soil_moisture_1;
    doc["soil_moisture_2"] = data.soil_moisture_2;
        // External temperatures removed
    // Flags detallados
    JsonObject flags = doc.createNestedObject("flags");
    flags["dht"] = sensors.isDhtValid();
    flags["soil_complete"] = sensors.isSoilComplete();
        flags["overall_complete"] = sensors.isDhtValid() && sensors.isSoilComplete();
    
    // Agregar estadísticas
    SystemStats stats = sensors.getStatistics();
    JsonObject statsObj = doc.createNestedObject("statistics");
    statsObj["temp_min"] = stats.temp_min;
    statsObj["temp_max"] = stats.temp_max;
    statsObj["temp_avg"] = stats.temp_avg;
    statsObj["humidity_min"] = stats.humidity_min;
    statsObj["humidity_max"] = stats.humidity_max;
    statsObj["humidity_avg"] = stats.humidity_avg;
    
    String response;
    serializeJson(doc, response);
    sendJSONResponse(request, 200, response);
}

void APIManager::handleGetSensorHistory(AsyncWebServerRequest* request) {
    if (!validateToken(request)) {
        sendErrorResponse(request, 401, "Unauthorized");
        return;
    }
    
    // Obtener parámetros de consulta
    unsigned long fromTime = 0;
    unsigned long toTime = system_manager.getCurrentTimestamp();
    
    if (request->hasParam("from")) {
        fromTime = request->getParam("from")->value().toInt();
    }
    if (request->hasParam("to")) {
        toTime = request->getParam("to")->value().toInt();
    }
    
    String historyData = database.getSensorHistory(fromTime, toTime);
    sendJSONResponse(request, 200, historyData);
}

void APIManager::handleCalibrateSensor(AsyncWebServerRequest* request) {
    if (!validateToken(request)) {
        sendErrorResponse(request, 401, "Unauthorized");
        return;
    }
    
    // Parsear datos del cuerpo de la request
    String body;
    if (request->hasParam("sensor", true) && request->hasParam("offset", true)) {
        int sensorIndex = request->getParam("sensor", true)->value().toInt();
        float offset = request->getParam("offset", true)->value().toFloat();
        
        if (sensorIndex >= 1 && sensorIndex <= 2) {
            sensors.setSoilMoistureOffset(sensorIndex, offset);
            
            StaticJsonDocument<128> doc;
            doc["success"] = true;
            doc["message"] = "Sensor calibrated successfully";
            
            String response;
            serializeJson(doc, response);
            sendJSONResponse(request, 200, response);
        } else {
            sendErrorResponse(request, 400, "Invalid sensor index");
        }
    } else {
        sendErrorResponse(request, 400, "Missing sensor or offset parameter");
    }
}

// ========== HANDLERS DE RELAYS ==========
void APIManager::handleGetRelays(AsyncWebServerRequest* request) {
    if (!validateToken(request)) {
        sendErrorResponse(request, 401, "Unauthorized");
        return;
    }
    
    String systemStatus = relays.getSystemStatus();
    sendJSONResponse(request, 200, systemStatus);
}

void APIManager::handleSetRelay(AsyncWebServerRequest* request) {
    if (!validateToken(request)) {
        sendErrorResponse(request, 401, "Unauthorized");
        return;
    }
    
    if (request->hasParam("relay", true) && request->hasParam("state", true)) {
        int relayIndex = request->getParam("relay", true)->value().toInt();
        bool state = request->getParam("state", true)->value().equals("true") || 
                    request->getParam("state", true)->value().equals("1");
        
        // Cambiar a modo manual para control directo
        relays.setRelayMode(relayIndex, RELAY_MODE_MANUAL);
        
        if (relays.setRelay(relayIndex, state)) {
            StaticJsonDocument<128> doc;
            doc["success"] = true;
            doc["relay"] = relayIndex;
            doc["state"] = state;
            doc["message"] = "Relay state updated successfully";
            
            String response;
            serializeJson(doc, response);
            sendJSONResponse(request, 200, response);
        } else {
            sendErrorResponse(request, 400, "Failed to set relay state");
        }
    } else {
        sendErrorResponse(request, 400, "Missing relay or state parameter");
    }
}

void APIManager::handleSetRelayMode(AsyncWebServerRequest* request) {
    if (!validateToken(request)) {
        sendErrorResponse(request, 401, "Unauthorized");
        return;
    }
    
    if (request->hasParam("relay", true) && request->hasParam("mode", true)) {
        int relayIndex = request->getParam("relay", true)->value().toInt();
        String modeStr = request->getParam("mode", true)->value();
        
        RelayMode mode = (modeStr.equals("auto")) ? RELAY_MODE_AUTO : RELAY_MODE_MANUAL;
        
        if (relays.setRelayMode(relayIndex, mode)) {
            StaticJsonDocument<128> doc;
            doc["success"] = true;
            doc["relay"] = relayIndex;
            doc["mode"] = modeStr;
            doc["message"] = "Relay mode updated successfully";
            
            String response;
            serializeJson(doc, response);
            sendJSONResponse(request, 200, response);
        } else {
            sendErrorResponse(request, 400, "Failed to set relay mode");
        }
    } else {
        sendErrorResponse(request, 400, "Missing relay or mode parameter");
    }
}

void APIManager::handleSetAutoRule(AsyncWebServerRequest* request) {
    if (!validateToken(request)) {
        sendErrorResponse(request, 401, "Unauthorized");
        return;
    }
    
    if (request->hasParam("relay", true) && request->hasParam("rule", true)) {
        int relayIndex = request->getParam("relay", true)->value().toInt();
        String ruleJson = request->getParam("rule", true)->value();
        
        if (relays.setAutoRule(relayIndex, ruleJson)) {
            StaticJsonDocument<128> doc;
            doc["success"] = true;
            doc["relay"] = relayIndex;
            doc["message"] = "Auto rule set successfully";
            
            String response;
            serializeJson(doc, response);
            sendJSONResponse(request, 200, response);
        } else {
            sendErrorResponse(request, 400, "Failed to set auto rule");
        }
    } else {
        sendErrorResponse(request, 400, "Missing relay or rule parameter");
    }
}

// ========== HANDLERS DEL SISTEMA ==========
void APIManager::handleGetSystemStatus(AsyncWebServerRequest* request) {
    if (!validateToken(request)) {
        sendErrorResponse(request, 401, "Unauthorized");
        return;
    }
    
    String systemInfo = system_manager.getSystemInfo();
    sendJSONResponse(request, 200, systemInfo);
}

void APIManager::handleGetStatistics(AsyncWebServerRequest* request) {
    if (!validateToken(request)) {
        sendErrorResponse(request, 401, "Unauthorized");
        return;
    }
    
    SystemStats sensorStats = sensors.getStatistics();
    SystemStats relayStats = relays.getRelayStatistics();
    
    StaticJsonDocument<1024> doc;
    
    // Estadísticas de sensores
    JsonObject sensors = doc.createNestedObject("sensors");
    sensors["temp_min"] = sensorStats.temp_min;
    sensors["temp_max"] = sensorStats.temp_max;
    sensors["temp_avg"] = sensorStats.temp_avg;
    sensors["humidity_min"] = sensorStats.humidity_min;
    sensors["humidity_max"] = sensorStats.humidity_max;
    sensors["humidity_avg"] = sensorStats.humidity_avg;
    sensors["soil_min"] = sensorStats.soil_min;
    sensors["soil_max"] = sensorStats.soil_max;
    sensors["soil_avg"] = sensorStats.soil_avg;
    
    // Estadísticas de relays
    JsonObject relaysObj = doc.createNestedObject("relays");
    relaysObj["heating_time"] = relayStats.heating_time;
    relaysObj["irrigation_time"] = relayStats.irrigation_time;
    
    // Información del sistema
    doc["uptime"] = system_manager.getUptime();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();
    
    String response;
    serializeJson(doc, response);
    sendJSONResponse(request, 200, response);
}

void APIManager::handleSystemPause(AsyncWebServerRequest* request) {
    if (!validateToken(request)) {
        sendErrorResponse(request, 401, "Unauthorized");
        return;
    }
    
    if (request->hasParam("pause", true)) {
        bool pause = request->getParam("pause", true)->value().equals("true") || 
                    request->getParam("pause", true)->value().equals("1");
        
        relays.pauseSystem(pause);
        
        StaticJsonDocument<128> doc;
        doc["success"] = true;
        doc["paused"] = pause;
        doc["message"] = pause ? "System paused" : "System resumed";
        
        String response;
        serializeJson(doc, response);
        sendJSONResponse(request, 200, response);
    } else {
        sendErrorResponse(request, 400, "Missing pause parameter");
    }
}

void APIManager::handleSystemReset(AsyncWebServerRequest* request) {
    if (!validateToken(request)) {
        sendErrorResponse(request, 401, "Unauthorized");
        return;
    }
    
    database.logSystemEvent("api_reset", "System reset requested via API");
    
    StaticJsonDocument<128> doc;
    doc["success"] = true;
    doc["message"] = "System will restart in 3 seconds";
    
    String response;
    serializeJson(doc, response);
    sendJSONResponse(request, 200, response);
    
    // Programar reinicio
    delay(100); // Dar tiempo para enviar la respuesta
    ESP.restart();
}

void APIManager::handleWiFiReset(AsyncWebServerRequest* request) {
    if (!validateToken(request)) {
        sendErrorResponse(request, 401, "Unauthorized");
        return;
    }
    
    database.logSystemEvent("wifi_reset", "WiFi configuration reset requested via API");
    
    StaticJsonDocument<128> doc;
    doc["success"] = true;
    doc["message"] = "WiFi configuration will be reset. Device will restart and create AP 'ESP32-Invernadero'";
    
    String response;
    serializeJson(doc, response);
    sendJSONResponse(request, 200, response);
    
    // Programar reset WiFi
    delay(100); // Dar tiempo para enviar la respuesta
    system_manager.resetWiFiConfig();
}

// ========== HANDLERS DE CONFIGURACIÓN ==========
void APIManager::handleGetConfig(AsyncWebServerRequest* request) {
    if (!validateToken(request)) {
        sendErrorResponse(request, 401, "Unauthorized");
        return;
    }
    
    StaticJsonDocument<512> doc;
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["build_date"] = BUILD_DATE;
    doc["api_port"] = API_PORT;
    doc["sensor_read_interval"] = SENSOR_READ_INTERVAL_MS;
    doc["log_interval"] = LOG_INTERVAL_MS;
    doc["safety_limits"]["max_temp"] = MAX_TEMP_CELSIUS;
    doc["safety_limits"]["min_temp"] = MIN_TEMP_CELSIUS;
    doc["safety_limits"]["max_humidity"] = MAX_HUMIDITY_PERCENT;
    doc["safety_limits"]["min_humidity"] = MIN_HUMIDITY_PERCENT;
    
    String response;
    serializeJson(doc, response);
    sendJSONResponse(request, 200, response);
}

void APIManager::handleSetConfig(AsyncWebServerRequest* request) {
    if (!validateToken(request)) {
        sendErrorResponse(request, 401, "Unauthorized");
        return;
    }
    // Placeholder: accept JSON body in future for dynamic config.
    sendErrorResponse(request, 501, "Configuration updates not implemented yet (schema versioning ready)");
}

void APIManager::handleBackupConfig(AsyncWebServerRequest* request) {
    if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
    StaticJsonDocument<512> doc;
    doc["schema_version"] = CONFIG_SCHEMA_VERSION;
    doc["firmware_version"] = FIRMWARE_VERSION;
    JsonObject safety = doc.createNestedObject("safety_limits");
    safety["max_temp"] = MAX_TEMP_CELSIUS;
    safety["min_temp"] = MIN_TEMP_CELSIUS;
    safety["max_humidity"] = MAX_HUMIDITY_PERCENT;
    safety["min_humidity"] = MIN_HUMIDITY_PERCENT;
    doc["sensor_read_interval"] = SENSOR_READ_INTERVAL_MS;
    doc["log_interval"] = LOG_INTERVAL_MS;
    // Future: include dynamic rule set, network settings, etc.
    String out; serializeJson(doc, out);
    sendJSONResponse(request, 200, out);
}

void APIManager::handleRestoreConfig(AsyncWebServerRequest* request) {
    if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
    if (restoreLen == 0) { sendErrorResponse(request, 400, "Empty body"); return; }
    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, restoreBuf, restoreLen);
    if (err) { sendErrorResponse(request, 400, String("JSON parse error: ")+err.c_str()); return; }
    uint32_t fromVer = 0;
    if (!migrateConfigSchema(doc, fromVer)) {
        sendErrorResponse(request, 400, "Unsupported or corrupt schema version");
        return;
    }
    // Placeholder: here we would apply values (currently read-only) and persist dynamic config.
    StaticJsonDocument<256> resp;
    resp["success"] = true;
    resp["from_version"] = fromVer;
    resp["to_version"] = CONFIG_SCHEMA_VERSION;
    resp["applied"] = false; // still read-only mode
    String out; serializeJson(resp, out);
    sendJSONResponse(request, 200, out);
    restoreLen = 0; // clear buffer
}

void APIManager::appendBodyChunk(AsyncWebServerRequest* request, uint8_t *data, size_t len, size_t index, size_t total) {
    (void)request; (void)total; // total could be used for validation
    if (index == 0) restoreLen = 0;
    size_t space = RESTORE_BUF_MAX - restoreLen;
    if (space == 0) return; // silently drop overflow
    if (len > space) len = space;
    memcpy(restoreBuf + restoreLen, data, len);
    restoreLen += len;
}

// ========== HANDLERS DE LOGS ==========
void APIManager::handleGetLogs(AsyncWebServerRequest* request) {
    if (!validateToken(request)) {
        sendErrorResponse(request, 401, "Unauthorized");
        return;
    }
    
    int count = 50; // Default
    if (request->hasParam("count")) {
        count = request->getParam("count")->value().toInt();
        count = constrain(count, 1, 1000);
    }

    // Prefer cloud when available, fallback to local
    String logs = database.getRecentLogs(count);
    sendJSONResponse(request, 200, logs);
}

void APIManager::handleClearLogs(AsyncWebServerRequest* request) {
    if (!validateToken(request)) {
        sendErrorResponse(request, 401, "Unauthorized");
        return;
    }
    
    if (database.clearLocalLogs()) {
        StaticJsonDocument<128> doc;
        doc["success"] = true;
        doc["message"] = "Local logs cleared successfully";
        
        String response;
        serializeJson(doc, response);
        sendJSONResponse(request, 200, response);
    } else {
        sendErrorResponse(request, 500, "Failed to clear logs");
    }
}

// ========== HANDLERS DE FIRMWARE ==========
void APIManager::handleGetFirmwareInfo(AsyncWebServerRequest* request) {
    if (!validateToken(request)) {
        sendErrorResponse(request, 401, "Unauthorized");
        return;
    }
    
    StaticJsonDocument<256> doc;
    doc["version"] = FIRMWARE_VERSION;
    doc["build_date"] = BUILD_DATE;
    doc["chip_model"] = ESP.getChipModel();
    doc["chip_revision"] = ESP.getChipRevision();
    doc["flash_size"] = ESP.getFlashChipSize();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["uptime"] = system_manager.getUptime();
    
    String response;
    serializeJson(doc, response);
    sendJSONResponse(request, 200, response);
}

// ========== UTILITIES ==========
bool APIManager::validateToken(AsyncWebServerRequest* request) {
    if (authToken.isEmpty()) return true;
    String provided;
    if (request->hasHeader("Authorization")) {
        String hdr = request->getHeader("Authorization")->value();
        const String prefix = "Bearer ";
        if (hdr.startsWith(prefix)) provided = hdr.substring(prefix.length());
    } else if (request->hasParam("token")) {
        provided = request->getParam("token")->value();
    } else return false;
    if (provided.isEmpty()) return false;
    uint8_t hash[32];
#ifdef MBEDTLS_MD_SHA256_C
    #include <mbedtls/sha256.h>
    mbedtls_sha256_context ctx; mbedtls_sha256_init(&ctx); mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, (const unsigned char*)provided.c_str(), provided.length());
    mbedtls_sha256_finish_ret(&ctx, hash); mbedtls_sha256_free(&ctx);
#else
    memset(hash,0,sizeof(hash)); for (size_t i=0;i<provided.length();++i) hash[i%32]^=provided[i];
#endif
    if (!tokenHashed) computeTokenHash();
    uint8_t diff=0; for (int i=0;i<32;i++) diff |= (hash[i]^authTokenHash[i]);
    if (diff!=0) return false;
#ifdef FEATURE_HMAC_AUTH
    // Optional HMAC validation (body-based). If header absent, accept (backwards compatible).
    if (request->hasHeader("X-Signature")) {
        String sigHex = request->getHeader("X-Signature")->value();
        // Reconstruct body from buffer if available (only for requests with body we processed) - not stored globally now.
        // Minimal placeholder: skip actual body retrieval (needs onBody). For future: compute HMAC over restoreBuf when using restore.
        // Accept signature placeholder until full body capture integrated for all endpoints.
    }
#endif
    return true;
}
bool APIManager::persistTokenHash() {
    if (!ensureFS()) return false;
    File f = LittleFS.open(TOKEN_HASH_FILE, "w");
    if (!f) return false;
    size_t w = f.write(authTokenHash, sizeof(authTokenHash));
    f.close();
    return w == sizeof(authTokenHash);
}

bool APIManager::loadPersistedTokenHash() {
    if (!ensureFS()) return false;
    if (!LittleFS.exists(TOKEN_HASH_FILE)) return false;
    File f = LittleFS.open(TOKEN_HASH_FILE, "r"); if (!f) return false;
    size_t r = f.read(authTokenHash, sizeof(authTokenHash));
    f.close();
    if (r != sizeof(authTokenHash)) return false;
    tokenHashed = true;
    return true;
}

bool APIManager::rotateToken(const String& current, const String& replacement) {
    if (replacement.length() < 12) return false; // mínimo razonable
    // Verificar actual
    String providedCurrent = current;
    if (providedCurrent.isEmpty()) return false;
    // Hash del current
    uint8_t hashCur[32];
#ifdef MBEDTLS_MD_SHA256_C
    {
        mbedtls_sha256_context ctx; mbedtls_sha256_init(&ctx); mbedtls_sha256_starts_ret(&ctx, 0);
        mbedtls_sha256_update_ret(&ctx, (const unsigned char*)providedCurrent.c_str(), providedCurrent.length());
        mbedtls_sha256_finish_ret(&ctx, hashCur); mbedtls_sha256_free(&ctx);
    }
#else
    memset(hashCur,0,sizeof(hashCur)); for (size_t i=0;i<providedCurrent.length();++i) hashCur[i%32]^=providedCurrent[i];
#endif
    uint8_t diff=0; for (int i=0;i<32;i++) diff |= (hashCur[i]^authTokenHash[i]);
    if (diff!=0) return false; // no coincide token actual

    // Hash del nuevo y persistir
#ifdef MBEDTLS_MD_SHA256_C
    {
        mbedtls_sha256_context ctx; mbedtls_sha256_init(&ctx); mbedtls_sha256_starts_ret(&ctx, 0);
        mbedtls_sha256_update_ret(&ctx, (const unsigned char*)replacement.c_str(), replacement.length());
        mbedtls_sha256_finish_ret(&ctx, authTokenHash); mbedtls_sha256_free(&ctx);
    }
#else
    memset(authTokenHash,0,sizeof(authTokenHash)); for (size_t i=0;i<replacement.length();++i) authTokenHash[i%32]^=replacement[i];
#endif
    tokenHashed = true;
    if (!persistTokenHash()) return false;
    return true;
}

void APIManager::handleRotateToken(AsyncWebServerRequest* request) {
    if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
    if (!request->hasParam("current", true) || !request->hasParam("next", true)) {
        sendErrorResponse(request, 400, "Missing current or next param"); return; }
    String cur = request->getParam("current", true)->value();
    String nxt = request->getParam("next", true)->value();
    if (!rotateToken(cur, nxt)) { sendErrorResponse(request, 400, "Rotation failed (bad current or weak next)"); return; }
    StaticJsonDocument<128> doc; doc["success"] = true; doc["note"] = "Token rotated; use new token immediately"; String out; serializeJson(doc, out); sendJSONResponse(request, 200, out);
}

// (definition moved to top)
bool APIManager::checkRateLimit(AsyncWebServerRequest* request) {
    IPAddress ip = request->client()->remoteIP();
    bool firstExceed=false;
    bool allowed = gRateLimiter.allow((uint32_t)ip, millis(), &firstExceed);
    if (!allowed && firstExceed) {
        database.logSystemEvent("rate_limit_exceeded", String("ip=")+ip.toString());
    }
    return allowed;
}

void APIManager::sendJSONResponse(AsyncWebServerRequest* request, int code, const String& json) {
    AsyncWebServerResponse *response = request->beginResponse(code, "application/json", json);
    setCORSHeaders(response);
    request->send(response);
}

void APIManager::sendErrorResponse(AsyncWebServerRequest* request, int code, const String& error) {
    String errorJson = createJSONError(error);
    sendJSONResponse(request, code, errorJson);
}

String APIManager::createJSONError(const String& error) {
    StaticJsonDocument<128> doc;
    doc["error"] = true;
    doc["message"] = error;
    doc["timestamp"] = system_manager.getCurrentTimestamp();
    
    String result;
    serializeJson(doc, result);
    return result;
}

void APIManager::setCORSHeaders(AsyncWebServerResponse* response) {
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

void APIManager::setAuthToken(const String& token) {
    authToken = token;
    computeTokenHash();
    database.logSystemEvent("auth_token_updated", "API authentication token updated");
}

void APIManager::handleClient() {
    // No-op: AsyncWebServer maneja conexiones internamente.
}

void APIManager::computeTokenHash() {
    if (authToken.isEmpty()) { tokenHashed=false; memset(authTokenHash,0,sizeof(authTokenHash)); return; }
#ifdef MBEDTLS_MD_SHA256_C
    #include <mbedtls/sha256.h>
    mbedtls_sha256_context ctx; mbedtls_sha256_init(&ctx); mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, (const unsigned char*)authToken.c_str(), authToken.length());
    mbedtls_sha256_finish_ret(&ctx, authTokenHash); mbedtls_sha256_free(&ctx);
#else
    memset(authTokenHash,0,sizeof(authTokenHash));
    for (size_t i=0;i<authToken.length(); ++i) authTokenHash[i%32]^=authToken[i];
#endif
    tokenHashed = true;
}

String APIManager::getRequestBody(AsyncWebServerRequest* request) {
    // Return accumulated body from restoreBuf (populated by appendBodyChunk)
    if (restoreLen > 0 && restoreLen < RESTORE_BUF_MAX) {
        return String(restoreBuf, restoreLen);
    }
    return "";
}

// ============================================
// Rule Engine API Handlers
// ============================================

#ifndef FEATURE_MINIMAL_API

void APIManager::handleGetRelayRules(AsyncWebServerRequest* request) {
    if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
    
    // Extract relay_id from path params
    String path = request->url();
    int relay_id = -1;
    if (sscanf(path.c_str(), "/api/relays/%d/rules", &relay_id) != 1 || relay_id < 0 || relay_id >= 4) {
        sendErrorResponse(request, 400, "Invalid relay ID");
        return;
    }
    
    String rules_json = ruleEngine.exportRules(relay_id);
    sendJSONResponse(request, 200, rules_json);
}

void APIManager::handleAddRelayRule(AsyncWebServerRequest* request) {
    if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
    
    String path = request->url();
    int relay_id = -1;
    if (sscanf(path.c_str(), "/api/relays/%d/rules", &relay_id) != 1 || relay_id < 0 || relay_id >= 4) {
        sendErrorResponse(request, 400, "Invalid relay ID");
        return;
    }
    
    String body = getRequestBody(request);
    if (body.isEmpty()) {
        sendErrorResponse(request, 400, "Empty body");
        return;
    }
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        sendErrorResponse(request, 400, "Invalid JSON");
        return;
    }
    
    Rule rule;
    if (!rule.fromJson(doc.as<JsonObject>())) {
        sendErrorResponse(request, 400, "Invalid rule format");
        return;
    }
    
    if (ruleEngine.addRule(relay_id, rule)) {
        StaticJsonDocument<128> response;
        response["success"] = true;
        response["message"] = "Rule added successfully";
        String out;
        serializeJson(response, out);
        sendJSONResponse(request, 201, out);
    } else {
        sendErrorResponse(request, 500, "Failed to add rule");
    }
}

void APIManager::handleUpdateRelayRule(AsyncWebServerRequest* request) {
    if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
    
    String path = request->url();
    int relay_id = -1, rule_idx = -1;
    if (sscanf(path.c_str(), "/api/relays/%d/rules/%d", &relay_id, &rule_idx) != 2 || 
        relay_id < 0 || relay_id >= 4 || rule_idx < 0) {
        sendErrorResponse(request, 400, "Invalid relay ID or rule index");
        return;
    }
    
    String body = getRequestBody(request);
    if (body.isEmpty()) {
        sendErrorResponse(request, 400, "Empty body");
        return;
    }
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        sendErrorResponse(request, 400, "Invalid JSON");
        return;
    }
    
    Rule rule;
    if (!rule.fromJson(doc.as<JsonObject>())) {
        sendErrorResponse(request, 400, "Invalid rule format");
        return;
    }
    
    if (ruleEngine.updateRule(relay_id, rule_idx, rule)) {
        StaticJsonDocument<128> response;
        response["success"] = true;
        response["message"] = "Rule updated successfully";
        String out;
        serializeJson(response, out);
        sendJSONResponse(request, 200, out);
    } else {
        sendErrorResponse(request, 500, "Failed to update rule");
    }
}

void APIManager::handleDeleteRelayRule(AsyncWebServerRequest* request) {
    if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
    
    String path = request->url();
    int relay_id = -1, rule_idx = -1;
    if (sscanf(path.c_str(), "/api/relays/%d/rules/%d", &relay_id, &rule_idx) != 2 || 
        relay_id < 0 || relay_id >= 4 || rule_idx < 0) {
        sendErrorResponse(request, 400, "Invalid relay ID or rule index");
        return;
    }
    
    if (ruleEngine.deleteRule(relay_id, rule_idx)) {
        StaticJsonDocument<128> response;
        response["success"] = true;
        response["message"] = "Rule deleted successfully";
        String out;
        serializeJson(response, out);
        sendJSONResponse(request, 200, out);
    } else {
        sendErrorResponse(request, 404, "Rule not found or failed to delete");
    }
}

void APIManager::handleClearRelayRules(AsyncWebServerRequest* request) {
    if (!validateToken(request)) { sendErrorResponse(request, 401, "Unauthorized"); return; }
    
    String path = request->url();
    int relay_id = -1;
    if (sscanf(path.c_str(), "/api/relays/%d/rules", &relay_id) != 1 || relay_id < 0 || relay_id >= 4) {
        sendErrorResponse(request, 400, "Invalid relay ID");
        return;
    }
    
    if (ruleEngine.clearRules(relay_id)) {
        StaticJsonDocument<128> response;
        response["success"] = true;
        response["message"] = "All rules cleared successfully";
        String out;
        serializeJson(response, out);
        sendJSONResponse(request, 200, out);
    } else {
        sendErrorResponse(request, 500, "Failed to clear rules");
    }
}

#endif // FEATURE_MINIMAL_API

// ============================================
// WebSocket Implementation
// ============================================

void APIManager::setupWebSocket() {
    ws = new AsyncWebSocket("/ws");
    
    ws->onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, 
                       AwsEventType type, void *arg, uint8_t *data, size_t len) {
        this->handleWebSocketEvent(server, client, type, arg, data, len);
    });
    
    server->addHandler(ws);
    DEBUG_PRINTLN(F("WebSocket configured on /ws"));
}

void APIManager::handleWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                                     AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            DEBUG_PRINTLN(String(F("WebSocket client #")) + String(client->id()) + F(" connected"));
            // Send current state to new client
            {
                StaticJsonDocument<512> doc;
                doc["type"] = "connected";
                doc["clientId"] = client->id();
                doc["timestamp"] = millis();
                String output;
                serializeJson(doc, output);
                client->text(output);
            }
            break;
            
        case WS_EVT_DISCONNECT:
            DEBUG_PRINTLN(String(F("WebSocket client #")) + String(client->id()) + F(" disconnected"));
            break;
            
        case WS_EVT_ERROR:
            DEBUG_PRINTLN(String(F("WebSocket client #")) + String(client->id()) + F(" error"));
            break;
            
        case WS_EVT_DATA:
            handleWebSocketMessage(client, data, len);
            break;
            
        case WS_EVT_PONG:
            // Pong received
            break;
    }
}

void APIManager::handleWebSocketMessage(AsyncWebSocketClient *client, uint8_t *data, size_t len) {
    // Parse incoming JSON message
    StaticJsonDocument<512> doc; // Reduced from DynamicJsonDocument(1024)
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        return; // Removed debug logging to save flash
    }
    
    const char* type = doc["type"];
    if (!type) {
        return; // Removed debug logging to save flash
    }
    
    // Authenticate WebSocket commands that modify state
    const char* token = doc["token"];
    bool authenticated = (token && String(token) == authToken);
    
    DynamicJsonDocument response(512);
    response["requestId"] = doc["id"] | 0;
    
    if (strcmp(type, "ping") == 0) {
        // Ping-pong for keepalive
        response["type"] = "pong";
        response["timestamp"] = millis();
        
    } else if (strcmp(type, "getSensors") == 0) {
        // Send current sensor data
        SensorData data = sensors.getCurrentData();
        response["type"] = "sensors";
        response["temp"] = data.temperature;
        response["humidity"] = data.humidity;
        response["soil"] = data.soil_moisture_1;
        response["timestamp"] = millis();
        
    } else if (strcmp(type, "getRelays") == 0) {
        // Send current relay states
        response["type"] = "relays";
        JsonArray relays = response.createNestedArray("data");
        for (int i = 0; i < 4; i++) {
            JsonObject relay = relays.createNestedObject();
            relay["id"] = i;
            relay["is_on"] = ::relays.getRelayState(i);
            relay["mode"] = ::relays.getRelayMode(i) == RELAY_MODE_AUTO ? "auto" : "manual";
        }
        response["timestamp"] = millis();
        
    } else if (strcmp(type, "setRelay") == 0) {
        if (!authenticated) {
            response["type"] = "error";
            response["error"] = "Unauthorized";
        } else {
            int relayId = doc["relay"] | -1;
            bool state = doc["state"] | false;
            
            if (relayId >= 0 && relayId < 4) {
                ::relays.setRelay(relayId, state);
                response["type"] = "relayState";
                response["relay"] = relayId;
                response["is_on"] = state;
                response["success"] = true;
                
                // Broadcast to all clients
                broadcastRelayState(relayId);
            } else {
                response["type"] = "error";
                response["error"] = "Invalid relay ID";
            }
        }
        
    } else if (strcmp(type, "setRelayMode") == 0) {
        if (!authenticated) {
            response["type"] = "error";
            response["error"] = "Unauthorized";
        } else {
            int relayId = doc["relay"] | -1;
            const char* mode = doc["mode"];
            
            if (relayId >= 0 && relayId < 4 && mode) {
                RelayMode newMode = (strcmp(mode, "auto") == 0) ? RELAY_MODE_AUTO : RELAY_MODE_MANUAL;
                ::relays.setRelayMode(relayId, newMode);
                response["type"] = "relayMode";
                response["relay"] = relayId;
                response["mode"] = mode;
                response["success"] = true;
                
                // Broadcast to all clients
                broadcastRelayState(relayId);
            } else {
                response["type"] = "error";
                response["error"] = "Invalid parameters";
            }
        }
        
    } else if (strcmp(type, "subscribe") == 0) {
        // Client wants to subscribe to real-time updates
        response["type"] = "subscribed";
        response["streams"] = doc["streams"]; // Echo back what they subscribed to
        
    } else {
        response["type"] = "error";
        response["error"] = "Unknown message type";
    }
    
    // Send response back to client
    String output;
    serializeJson(response, output);
    client->text(output);
}

// Broadcast functions
void APIManager::broadcastSensorData() {
    if (!ws) return;
    
    SensorData data = sensors.getCurrentData();
    StaticJsonDocument<256> doc;
    doc["type"] = "sensors";
    doc["temp"] = data.temperature;
    doc["humidity"] = data.humidity;
    doc["soil"] = data.soil_moisture_1;
    doc["timestamp"] = millis();
    
    String output;
    serializeJson(doc, output);
    ws->textAll(output);
}

void APIManager::broadcastRelayState(int relayId) {
    if (!ws || relayId < 0 || relayId >= 4) return;
    
    StaticJsonDocument<256> doc;
    doc["type"] = "relayState";
    doc["relay"] = relayId;
    doc["is_on"] = ::relays.getRelayState(relayId);
    doc["mode"] = ::relays.getRelayMode(relayId) == RELAY_MODE_AUTO ? "auto" : "manual";
    doc["timestamp"] = millis();
    
    String output;
    serializeJson(doc, output);
    ws->textAll(output);
}

void APIManager::broadcastSystemStatus() {
    if (!ws) return;
    
    StaticJsonDocument<512> doc;
    doc["type"] = "systemStatus";
    doc["uptime"] = system_manager.getUptime();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["wifiRSSI"] = WiFi.RSSI();
    doc["timestamp"] = millis();
    
    String output;
    serializeJson(doc, output);
    ws->textAll(output);
}

void APIManager::broadcastRuleEvent(int relayId, const String& ruleName, const String& action) {
    if (!ws) return;
    
    StaticJsonDocument<256> doc;
    doc["type"] = "ruleEvent";
    doc["relay"] = relayId;
    doc["rule"] = ruleName;
    doc["action"] = action;
    doc["timestamp"] = millis();
    
    String output;
    serializeJson(doc, output);
    ws->textAll(output);
}

void APIManager::sendToClient(uint32_t clientId, const String& message) {
    if (!ws) return;
    
    AsyncWebSocketClient* client = ws->client(clientId);
    if (client && client->status() == WS_CONNECTED) {
        client->text(message);
    }
}