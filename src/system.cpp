#include <Arduino.h>
#include "system.h"
#include "database.h"
#include <Preferences.h>
#include <WiFi.h>
#include <nvs_flash.h>
#include <esp_task_wdt.h>
#include <ArduinoJson.h>
#include "metrics.h"
#include <esp_heap_caps.h>
#include <NTPClient.h>
#include "nvs_utils.h"


SystemManager system_manager;

SystemManager::SystemManager() {
    ntpUDP = nullptr;
    ntpClient = nullptr;
    currentState = SYSTEM_INITIALIZING;
    bootTime = 0;
    lastHeartbeat = 0;
    bootEpoch = 0;
    bootMillis = 0;
    pauseButtonPressed = false;
    lastPauseButtonState = false;
    pauseButtonDebounceTime = 0;
    statusLedLastBlink = 0;
    statusLedState = false;
    statusLedEnabled = false;
}

SystemManager::~SystemManager() {
    if (ntpClient) delete ntpClient;
    if (ntpUDP) delete ntpUDP;
}

bool SystemManager::begin() {
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTLN(F("System init"));
#endif
    setupPins();
    loadSystemState();
    bootMillis = millis();
    bootTime = bootMillis;
    if (!initWiFi()) {
#if MIN_LOG_LEVEL <= LOG_ERROR
    DEBUG_PRINTLN(F("WiFi initialization failed"));
#endif
    }
    if (secretsArePlaceholder()) {
#if MIN_LOG_LEVEL <= LOG_WARNING
    DEBUG_PRINTLN(F("[WARNING] API_TOKEN placeholder en build. Reemplace en secrets.h para producción."));
#endif
        database.logSystemEvent("secrets_warning", "API_TOKEN placeholder en uso");
    }
    initNTP(); // continuar aunque falle
    initOTA(); // continuar aunque falle
    if (!initWatchdog()) {
#if MIN_LOG_LEVEL <= LOG_ERROR
    DEBUG_PRINTLN(F("Watchdog initialization failed"));
#endif
        return false;
    }
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTLN(F("System ready"));
#endif
    currentState = SYSTEM_NORMAL;
    return true;
}

void SystemManager::setupPins() {
    // Configurar pines de entrada
    pinMode(PAUSE_BUTTON_PIN, INPUT_PULLUP);
    
    // Configurar pines de salida
    // Los pines de LED y buzzer pueden compilarse condicionalmente.
    #ifndef ENABLE_STATUS_LED
    // STATUS LED deshabilitado en build (definir ENABLE_STATUS_LED para habilitar)
    #else
    pinMode(STATUS_LED_PIN, OUTPUT);
    #endif

    // Buzzer eliminado: no configurar pin
    
    // Inicializar estados
    // Ensure LED is off at startup. If ENABLE_STATUS_LED is defined at build time,
    // we still keep it off unless explicitly enabled at runtime.
    #ifdef ENABLE_STATUS_LED
        LED_WRITE_OFF(STATUS_LED_PIN);
    #endif

    // Buzzer eliminado
}




void SystemManager::update() {
    // Medir duración de ciclo anterior (si lastLoopMicros != 0)
    unsigned long nowUs = micros();
    if (lastLoopMicros != 0) {
        unsigned long delta = nowUs - lastLoopMicros;
        lastLoopDeltaUs = delta;
    // EMA loop time (configurable α)
    loopEmaUs = emaUpdate(loopEmaUs, (float)delta, LOOP_EMA_ALPHA);
    }
    lastLoopMicros = nowUs;

    // Actualizar mínimo de heap observado
    uint32_t fh = ESP.getFreeHeap();
    if (fh < minFreeHeap) minFreeHeap = fh;
    // Fragmentación: mayor bloque libre (requiere heap_caps)
#ifdef ESP_PLATFORM
    uint32_t lfb = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    largestFreeBlock = lfb;
    if (lfb < minLargestFreeBlock) minLargestFreeBlock = lfb;
    if (fh > 0) {
        fragmentationRatio = (float)lfb / (float)fh;
        if (minFragmentationRatio == 0.0f || fragmentationRatio < minFragmentationRatio) minFragmentationRatio = fragmentationRatio;
    }
#endif

    updatePauseButton();
    updateStatusLed();
    // updateBuzzer eliminado (buzzer removido)
    
    // Verificar salud del heap (crítico para operación a largo plazo)
    checkHeapHealth();
    
    // Verificar reinicio programado
    checkScheduledRestart();
    
    // Gestión no bloqueante de reconexión WiFi
    updateWifiReconnect(); // implemented in system_wifi.cpp
    
    // Actualizar NTP si es necesario
    if (ntpClient && isWiFiConnected()) {
        ntpClient->update();
    }
}

bool SystemManager::isPauseButtonPressed() { return pauseButtonPressed; }

void SystemManager::updateStatusLed() {
#ifdef ENABLE_STATUS_LED
    // Prioridad de patrones:
    // 1. Si WiFi conectado y no se solicitó blink manual -> LED sólido ON
    // 2. Si WiFi en reconexión -> parpadeo rápido (250 ms)
    // 3. Si blink manual habilitado -> lento (1000 ms) o rápido si estado SYSTEM_ERROR

    if (isWiFiConnected() && !statusLedEnabled) {
        // LED sólido indica conectividad estable
    if (!statusLedState) { statusLedState = true; LED_WRITE_ON(STATUS_LED_PIN); }
        return;
    }

    unsigned long now = millis();
    unsigned long interval = 1000; // default

    // Detectar reconexión WiFi (cuando no está conectado y en estados distintos de IDLE)
    if (!isWiFiConnected()) {
        // Parpadeo según última razón conocida
        uint8_t r = getLastWifiDisconnectReason();
        if (r == 202) { // AUTH_FAIL
            interval = 150; // muy rápido para auth
        } else if (r == 201) { // NO_AP_FOUND
            interval = 400; // medio
        } else {
            interval = 250; // default reconnect
        }
    } else if (currentState == SYSTEM_ERROR) {
        interval = 300; // error: más rápido que normal
    } else if (statusLedEnabled) {
        interval = 1000; // heartbeat normal
    } else {
        // Sin blink manual y conectado ya atendido arriba; si llega aquí apagamos
    if (statusLedState) { statusLedState = false; LED_WRITE_OFF(STATUS_LED_PIN); }
        return;
    }

    if (now - statusLedLastBlink >= interval) {
        statusLedLastBlink = now;
        statusLedState = !statusLedState;
    if (statusLedState) LED_WRITE_ON(STATUS_LED_PIN); else LED_WRITE_OFF(STATUS_LED_PIN);
    }
#endif
}

void SystemManager::setStatusLed(bool fast_blink) {
#ifdef ENABLE_STATUS_LED
    statusLedEnabled = true;
    // Optionally adapt blink pattern speed externally
    if (fast_blink) statusLedLastBlink = 0; // force immediate toggle
#endif
}

void SystemManager::disableStatusLed() { statusLedEnabled = false; }
void SystemManager::enableStatusLed() { statusLedEnabled = true; }

void SystemManager::handleOTA() { /* no-op (OTA gestionado externamente) */ }

float SystemManager::getLoopAvgMicros() { return loopEmaUs; }
unsigned long SystemManager::getLastLoopMicros() { return lastLoopDeltaUs; }

void SystemManager::updatePauseButton() {
    bool currentButtonState = digitalRead(PAUSE_BUTTON_PIN) == LOW; // Pullup, por eso LOW = presionado
    
    // Debounce del botón
    if (currentButtonState != lastPauseButtonState) {
        pauseButtonDebounceTime = millis();
    }
    
    if ((millis() - pauseButtonDebounceTime) > 50) { // 50ms de debounce
        if (currentButtonState && !pauseButtonPressed) {
            pauseButtonPressed = true;
        } else if (!currentButtonState && pauseButtonPressed) {
            pauseButtonPressed = false;
        }
    }
    
    lastPauseButtonState = currentButtonState;
}



// playBuzzer / updateBuzzer removidos (sin hardware)

void SystemManager::setState(SystemState state) {
    if (currentState != state) {
        SystemState oldState = currentState;
        currentState = state;
        
        const char* newStateStr = getStateString().c_str();
        char logBuf[96];
        snprintf(logBuf, sizeof(logBuf), "State changed from %d to %d", (int)oldState, (int)state);
        DEBUG_PRINT("System state changed: ");
    DEBUG_PRINTLN(newStateStr);
        database.logSystemEvent("state_change", String(logBuf));
    }
}

SystemState SystemManager::getState() {
    return currentState;
}

String SystemManager::getStateString() {
    switch (currentState) {
        case SYSTEM_INITIALIZING: return "Initializing";
        case SYSTEM_NORMAL: return "Normal";
        case SYSTEM_PAUSED: return "Paused";
        case SYSTEM_ERROR: return "Error";
        case SYSTEM_MAINTENANCE: return "Maintenance";
        default: return "Unknown";
    }
}




// feedWatchdog implementation in system_watchdog.cpp

unsigned long SystemManager::getUptime() {
    // Usar millis relativos desde el inicio del boot
    return millis() - bootMillis;
}

String SystemManager::getSystemInfo() {
    StaticJsonDocument<512> doc;
    
    doc["version"] = FIRMWARE_VERSION;
    doc["build_date"] = BUILD_DATE;
    doc["uptime"] = getUptime();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["chip_model"] = ESP.getChipModel();
    doc["chip_revision"] = ESP.getChipRevision();
    doc["flash_size"] = ESP.getFlashChipSize();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["ip_address"] = WiFi.localIP().toString();
    doc["mac_address"] = WiFi.macAddress();
    doc["current_time"] = getCurrentTimeString();
    doc["state"] = getStateString();
    
    String result;
    serializeJson(doc, result);
    return result;
}

void SystemManager::loadSystemState() {
    Preferences prefs;
    if (!safePrefsBegin(prefs, "system", true)) {
        return; // No NVS namespace yet; benign
    }
    // Verificar si hubo un reinicio inesperado
    if (prefs.getBool("power_lost", false)) {
        database.logSystemEvent("power_loss_detected", "Previous power loss detected");
    }
    prefs.end();
}

void SystemManager::saveSystemState() {
    Preferences prefs;
    if (!safePrefsBegin(prefs, "system", false)) return;
    prefs.putBool("power_lost", true);
    prefs.putULong("last_timestamp", getCurrentTimestamp());
    prefs.end();
}

bool SystemManager::wasPowerLost() {
    Preferences prefs;
    if (!safePrefsBegin(prefs, "system", true)) return false;
    bool powerLost = prefs.getBool("power_lost", false);
    prefs.end();
    return powerLost;
}

void SystemManager::handlePowerRestoration() {
    Preferences prefs;
    if (!safePrefsBegin(prefs, "system", false)) return;
    unsigned long lastTimestamp = prefs.getULong("last_timestamp", 0);
    unsigned long currentTimestamp = getCurrentTimestamp();
    if (lastTimestamp > 0) {
        unsigned long outageTime = currentTimestamp - lastTimestamp;
    // Delegar al módulo de eventos (prototipo externo)
    extern void SystemEvents_logPowerRestored(unsigned long);
    SystemEvents_logPowerRestored(outageTime);
    }
    
    prefs.putBool("power_lost", false);
    prefs.end();
}

void SystemManager::sendHeartbeat() {
    extern void SystemEvents_logHeartbeat();
    SystemEvents_logHeartbeat();
    lastHeartbeat = millis();
}

void SystemManager::checkHeapHealth() {
    static unsigned long lastCheck = 0;
    unsigned long now = millis();
    
    // Verificar cada 10 segundos
    if (now - lastCheck < 10000) return;
    lastCheck = now;
    
    uint32_t freeHeap = ESP.getFreeHeap();
    const uint32_t CRITICAL_HEAP = 30000; // 30KB umbral crítico
    const uint32_t WARNING_HEAP = 50000;  // 50KB umbral advertencia
    
    if (freeHeap < CRITICAL_HEAP) {
        // Situación crítica: limpiar buffers
        extern SensorManager sensors;
        extern DatabaseManager database;
        
        database.log(LOG_CRITICAL, "heap", "Critical heap detected - forcing cleanup", 
                    String("{\"free\":") + freeHeap + "}");
        
        // Forzar envío de logs para liberar buffer
        database.sendLogBuffer();
        
        // Resetear estadísticas de sensores
        sensors.resetStatistics();
        
#if MIN_LOG_LEVEL <= LOG_CRITICAL
        DEBUG_PRINTF("[CRITICAL] Heap=%u bytes - cleanup forced\n", freeHeap);
#endif
    } else if (freeHeap < WARNING_HEAP) {
#if MIN_LOG_LEVEL <= LOG_WARNING
        static unsigned long lastWarn = 0;
        if (now - lastWarn > 300000) { // Cada 5 min
            DEBUG_PRINTF("[WARNING] Low heap=%u bytes\n", freeHeap);
            database.log(LOG_WARNING, "heap", "Low heap detected", 
                        String("{\"free\":") + freeHeap + "}");
            lastWarn = now;
        }
#endif
    }
}

void SystemManager::checkScheduledRestart() {
    static unsigned long lastCheck = 0;
    static bool restartScheduled = false;
    unsigned long now = millis();
    
    // Verificar cada minuto
    if (now - lastCheck < 60000) return;
    lastCheck = now;
    
    unsigned long uptime = getUptime();
    
    // Solo reiniciar si uptime > 23 horas (evitar loops)
    if (uptime < 82800) { // 23h en segundos
        restartScheduled = false;
        return;
    }
    
    // Verificar si es hora de reiniciar (3:00 AM)
    if (isTimeSync()) {
        time_t now_t = getCurrentTimestamp();
        struct tm timeinfo;
        localtime_r(&now_t, &timeinfo);
        
        // Reiniciar entre 3:00 y 3:05 AM
        if (timeinfo.tm_hour == 3 && timeinfo.tm_min < 5 && !restartScheduled) {
            restartScheduled = true;
            database.log(LOG_INFO, "system", "Scheduled daily restart", 
                        String("{\"uptime\":") + uptime + "}");
            
#if MIN_LOG_LEVEL <= LOG_INFO
            DEBUG_PRINTLN(F("Performing scheduled daily restart..."));
#endif
            // Dar tiempo para enviar el log
            delay(2000);
            ESP.restart();
        } else if (timeinfo.tm_hour != 3) {
            restartScheduled = false;
        }
    }
}

// WiFiManager callbacks eliminados