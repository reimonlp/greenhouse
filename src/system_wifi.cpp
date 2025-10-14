#include "system.h"
#include "database.h"
#include <WiFi.h>

#ifdef ENABLE_WIFI_DEBUG
static const char* wifiReasonToStr(uint8_t r) {
    switch(r) {
        case 1: return "UNSPECIFIED"; // generic
        case 2: return "AUTH_EXPIRE";
        case 3: return "AUTH_LEAVE";
        case 4: return "ASSOC_EXPIRE";
        case 5: return "ASSOC_TOOMANY";
        case 6: return "NOT_AUTHED";
        case 7: return "NOT_ASSOCED";
        case 8: return "ASSOC_LEAVE";
        case 9: return "ASSOC_NOT_AUTHED";
        case 200: return "BEACON_TIMEOUT";
        case 201: return "NO_AP_FOUND";
        case 202: return "AUTH_FAIL";
        case 203: return "ASSOC_FAIL";
        case 204: return "HANDSHAKE_TIMEOUT";
        default: return "UNKNOWN";
    }
}
static void logWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch(event) {
        case ARDUINO_EVENT_WIFI_STA_START: DEBUG_PRINTLN(F("[WiFiEvt] STA_START")); break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED: DEBUG_PRINTLN(F("[WiFiEvt] STA_CONNECTED (auth OK, waiting DHCP)")); break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            DEBUG_PRINT(F("[WiFiEvt] STA_DISCONNECTED reason="));
            DEBUG_PRINT(info.wifi_sta_disconnected.reason);
            DEBUG_PRINT(' ');
            DEBUG_PRINTLN(wifiReasonToStr(info.wifi_sta_disconnected.reason));
            {
                // Increment global counter via system_manager (forward declared extern)
                extern SystemManager system_manager;
                system_manager.incrementWifiReason(info.wifi_sta_disconnected.reason);
            }
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            DEBUG_PRINT(F("[WiFiEvt] GOT_IP: "));
            DEBUG_PRINTLN(IPAddress(info.got_ip.ip_info.ip.addr));
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP: DEBUG_PRINTLN(F("[WiFiEvt] LOST_IP")); break;
        default: DEBUG_PRINT(F("[WiFiEvt] Event=")); DEBUG_PRINTLN((int)event); break;
    }
}
#endif

// WiFi-related functionality extracted from system.cpp

bool SystemManager::initWiFi() {
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTLN(F("Initializing WiFi (static credentials)..."));
#endif
#ifdef WIFI_INITIAL_DELAY_MS
    if (WIFI_INITIAL_DELAY_MS > 0) {
        delay(WIFI_INITIAL_DELAY_MS);
    }
#endif
    WiFi.mode(WIFI_STA);
#ifdef ENABLE_WIFI_DEBUG
    WiFi.onEvent([](WiFiEvent_t e, WiFiEventInfo_t info){ logWifiEvent(e, info); });
    DEBUG_PRINT(F("Using SSID: ")); DEBUG_PRINTLN(WIFI_SSID);
#endif
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long start = millis();
    unsigned long connectTimeout = WIFI_CONNECT_TIMEOUT_MS; // local copy (macro constant)
    bool firstAuthFail = false;
    bool fastRecovered = false;
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < connectTimeout) {
#ifdef ENABLE_WIFI_DEBUG
        wl_status_t st = WiFi.status();
        if ((millis() - start) % 1000 < 250) {
            DEBUG_PRINT(F("... waiting (status=")); DEBUG_PRINT((int)st); DEBUG_PRINTLN(')');
        }
#endif
        // Fast path: if we already got a disconnection event with AUTH_FAIL, attempt one immediate clean retry
#ifdef ENABLE_WIFI_DEBUG
        // We rely on event callback having printed reason=202 AUTH_FAIL; poll internal reason via wifi_reason not public; so just attempt one retry after ~1s if not connected
#endif
        delay(250);
        if (!firstAuthFail && WiFi.status() != WL_CONNECTED) {
            // heuristic: after 1 second with status connect failed, do one forced disconnect/retry
            if (millis() - start > 1000) {
                // Only do this once to avoid loop spam
                firstAuthFail = true;
                WiFi.disconnect(true, true); // drop config & flush
                delay(200);
                WiFi.mode(WIFI_STA);
                WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
                // extend timeout slightly to allow second attempt
                connectTimeout += 1500; // extend local timeout window for second attempt
            }
        }
    }
    if (WiFi.status() == WL_CONNECTED) {
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINT(F("WiFi connected. IP: ")); DEBUG_PRINTLN(WiFi.localIP());
#endif
        // One-shot confirmation blink if LED available
#ifdef ENABLE_STATUS_LED
        // Quick triple blink then leave ON
        LED_WRITE_OFF(STATUS_LED_PIN); delay(60);
        LED_WRITE_ON(STATUS_LED_PIN); delay(60);
        LED_WRITE_OFF(STATUS_LED_PIN); delay(60);
        LED_WRITE_ON(STATUS_LED_PIN); // solid after sequence
#endif
        if (firstAuthFail) {
            fastRecovered = true;
#if MIN_LOG_LEVEL <= LOG_WARNING
            DEBUG_PRINTLN(F("Recovered from initial AUTH_FAIL with fast retry"));
#endif
        }
        return true;
    }
#if MIN_LOG_LEVEL <= LOG_WARNING
    if (!fastRecovered) {
        DEBUG_PRINTLN(F("Initial WiFi connection failed (will retry asynchronously)"));
    }
#endif
    return false;
}

bool SystemManager::isWiFiConnected() {
    if (WiFi.status() != WL_CONNECTED) return false;
    IPAddress ip = WiFi.localIP();
    if (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) return false;
    return true;
}

void SystemManager::updateWifiReconnect() {
    if (isWiFiConnected()) {
        // Encender LED fijo cuando hay WiFi si está habilitado en build
#ifdef ENABLE_STATUS_LED
    if (!statusLedEnabled) { LED_WRITE_ON(STATUS_LED_PIN); }
#endif
        if (wifiReconnectState != WIFI_IDLE) {
            wifiReconnectState = WIFI_IDLE;
            wifiRetryCount = 0;
            wifiBackoffMs = 5000;
            wifiLostTimestamp = 0;
            autonomousMode = false;
            setState(SYSTEM_NORMAL);
            database.logSystemEvent("wifi_reconnect", "WiFi reconnected successfully");
            wifiReconnectSuccesses++;
        }
        return;
    }

    if (wifiReconnectState == WIFI_IDLE) {
#if MIN_LOG_LEVEL <= LOG_WARNING
    DEBUG_PRINTLN(F("WiFi lost - entering reconnect sequence"));
#endif
    wifiReconnectState = WIFI_LOST;
    wifiLostTimestamp = millis();
#ifdef ENABLE_STATUS_LED
    if (!statusLedEnabled) { LED_WRITE_OFF(STATUS_LED_PIN); }
#endif
        setState(SYSTEM_ERROR);
        wifiRetryStart = millis();
        return;
    }

    unsigned long now = millis();
    
    // Activar modo autónomo si WiFi perdido por >1 hora
    if (!autonomousMode && wifiLostTimestamp > 0 && (now - wifiLostTimestamp) > 3600000) {
        autonomousMode = true;
        database.logSystemEvent("autonomous_mode", "Entering autonomous mode - WiFi lost >1h");
#if MIN_LOG_LEVEL <= LOG_WARNING
        DEBUG_PRINTLN(F("Entering autonomous mode - WiFi lost >1 hour"));
#endif
    }
    
    switch (wifiReconnectState) {
        case WIFI_LOST:
            if (now - wifiRetryStart >= 1000) {
                wifiReconnectState = WIFI_RECONNECTING;
            }
            break;
        case WIFI_RECONNECTING: {
            WiFi.disconnect();
            WiFi.mode(WIFI_STA);
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            wifiRetryCount++;
            if (wifiRetryCount > wifiMaxRetries) {
                wifiBackoffMs = (wifiBackoffMs * 2 > wifiBackoffMax) ? wifiBackoffMax : wifiBackoffMs * 2;
                wifiRetryCount = 0;
            }
            // En modo autónomo, reducir frecuencia de reintentos
            unsigned long effectiveBackoff = autonomousMode ? 900000 : wifiBackoffMs; // 15min en autónomo
            wifiRetryStart = now;
            wifiReconnectState = WIFI_RETRY_WAIT;
            wifiReconnectAttempts++;
            break; }
        case WIFI_RETRY_WAIT: {
            if (isWiFiConnected()) {
                // handled next loop
            } else {
                unsigned long effectiveBackoff = autonomousMode ? 900000 : wifiBackoffMs;
                if (now - wifiRetryStart >= effectiveBackoff) {
                    wifiReconnectState = WIFI_RECONNECTING;
                }
            }
            break; }
        case WIFI_IDLE:
        default: break;
    }
}

void SystemManager::resetWiFiConfig() {
    DEBUG_PRINTLN(F("Resetting WiFi configuration..."));
    WiFi.disconnect(true);
    delay(1000);
    delay(2000);
    DEBUG_PRINTLN(F("WiFi configuration reset. Restarting..."));
    ESP.restart();
}
