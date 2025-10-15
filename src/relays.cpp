#include "relays.h"
#include "system.h"
#include "database.h"
#include "api.h"
#include <Preferences.h>
#include <time.h>
#include "time_source.h"
#include "relay_timeouts.h"
#include "relay_state_store.h"
#include "nvs_utils.h"
#include "rule_engine.h"

RelayManager relays;

// Definiciones PROGMEM (ver relays.h)
const char RelayManager::relayName0[] PROGMEM = "luces";
const char RelayManager::relayName1[] PROGMEM = "ventilador";
const char RelayManager::relayName2[] PROGMEM = "bomba";
const char RelayManager::relayName3[] PROGMEM = "calefactor";
const char* const RelayManager::relayNames_P[4] PROGMEM = {
    RelayManager::relayName0,
    RelayManager::relayName1,
    RelayManager::relayName2,
    RelayManager::relayName3
};

String RelayManager::relayName(uint8_t idx) const {
    if (idx >= 4) return String("?");
    char buffer[16];
    strncpy_P(buffer, (PGM_P)pgm_read_ptr(&relayNames_P[idx]), sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';
    return String(buffer);
}

RelayManager::RelayManager() {
    lastAutoCheck = 0;
    safetyLimitsEnabled = true;
    systemPaused = false;
    pauseStartTime = 0;
    
    // Inicializar estados de relays
    for (int i = 0; i < 4; i++) {
        relayStates[i] = {false, RELAY_MODE_AUTO, 0, 0, ""};
    autoRules[i].enabled = false;
    autoRules[i].type = "";
    autoRules[i].condition = "";
    autoRules[i].value1 = 0.0f;
    autoRules[i].value2 = 0.0f;
    autoRules[i].schedule = "";
    autoRules[i].duration = 0;
    autoRules[i].lastActivation = 0;
    autoRules[i].isActive = false;
    autoRules[i].lastEvalAt = 0;
    autoRules[i].lastDecision = false;
    autoRules[i].currentBucket = 0;
    autoRules[i].bucketBaseMinutes = 0;
    for (int b=0;b<6;++b){ autoRules[i].evalBuckets[b]=0; autoRules[i].actBuckets[b]=0; }
    }
}

bool RelayManager::begin() {
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTLN(F("Initializing relay manager..."));
#endif
    
    // Configurar pines como salidas
    for (int i = 0; i < 4; i++) {
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], LOW); // Relays activos en HIGH
    }
    
    // Cargar configuración desde NVS
    loadStateFromNVS();
    
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTLN(F("Relay manager initialized successfully"));
#endif
    return true;
}

void RelayManager::update() {
    if (systemPaused) return;
    
    // Procesar reglas automáticas cada 5 segundos
    if (timeSource().millis() - lastAutoCheck >= 5000) {
        processAutoRules();
        enforceTimeouts();
        lastAutoCheck = timeSource().millis();
    }
}

bool RelayManager::setRelay(int relayIndex, bool state) {
    if (relayIndex < 0 || relayIndex >= 4) {
    char err[48];
    snprintf(err, sizeof(err), "Invalid relay index: %d", relayIndex);
    lastError = err;
        return false;
    }
    
    if (systemPaused) {
        lastError = "System is paused";
        return false;
    }
    
    // Verificar límites de seguridad
    if (state && safetyLimitsEnabled && !checkSafetyLimits()) {
        lastError = "Safety limits would be exceeded";
        return false;
    }
    
    bool previousState = relayStates[relayIndex].is_on;
    
    // Actualizar estado físico
    digitalWrite(relayPins[relayIndex], state ? HIGH : LOW);

    // Actualizar tiempo total encendido: si estamos apagando (state==false) y
    // el relay estaba encendido anteriormente, sumar el intervalo usando el
    // last_change anterior (calcular antes de sobrescribir last_change).
    if (!state && previousState) {
        unsigned long now = timeSource().millis();
        unsigned long duration = 0;
        if (now >= relayStates[relayIndex].last_change) {
            duration = now - relayStates[relayIndex].last_change;
        }
        relayStates[relayIndex].total_on_time += duration;
    }

    // Actualizar estado interno
    relayStates[relayIndex].is_on = state;
    relayStates[relayIndex].last_change = timeSource().millis();
    
    // Log del evento
    String reason = relayStates[relayIndex].mode == RELAY_MODE_MANUAL ? 
                   "Manual control" : "Automatic rule";
    database.logRelayAction(relayIndex, state, relayStates[relayIndex].mode, reason);
    
    
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTF("R%s[%d]=%s %s\n", relayName(relayIndex).c_str(), relayIndex, state?"ON":"OFF", reason.c_str());
#endif
    
    // Broadcast state change via WebSocket
    api.broadcastRelayState(relayIndex);
    
    return true;
}

bool RelayManager::toggleRelay(int relayIndex) {
    if (relayIndex < 0 || relayIndex >= 4) return false;
    return setRelay(relayIndex, !relayStates[relayIndex].is_on);
}

bool RelayManager::getRelayState(int relayIndex) {
    if (relayIndex < 0 || relayIndex >= 4) return false;
    return relayStates[relayIndex].is_on;
}

RelayState RelayManager::getRelayStateStruct(int relayIndex) {
    static RelayState empty = {false, RELAY_MODE_MANUAL, 0, 0, ""};
    if (relayIndex < 0 || relayIndex >= 4) return empty;
    return relayStates[relayIndex];
}

bool RelayManager::setRelayMode(int relayIndex, RelayMode mode) {
    if (relayIndex < 0 || relayIndex >= 4) {
        lastError = "Invalid relay index";
        return false;
    }
    
    relayStates[relayIndex].mode = mode;
    
    String modeStr = (mode == RELAY_MODE_MANUAL) ? "Manual" : "Automatic";
    database.logSystemEvent("relay_mode_change", 
        "Relay " + relayName(relayIndex) + " mode changed to " + modeStr);
    
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTF("R%s mode %s\n", relayName(relayIndex).c_str(), modeStr.c_str());
#endif
    
    // Broadcast mode change via WebSocket
    api.broadcastRelayState(relayIndex);
    
    return true;
}

RelayMode RelayManager::getRelayMode(int relayIndex) {
    if (relayIndex < 0 || relayIndex >= 4) return RELAY_MODE_MANUAL;
    return relayStates[relayIndex].mode;
}

bool RelayManager::setAutoRule(int relayIndex, const String& ruleJson) {
    if (relayIndex < 0 || relayIndex >= 4) {
        lastError = "Invalid relay index";
        return false;
    }
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, ruleJson);
    
    if (error) {
    char err[96];
    snprintf(err, sizeof(err), "Invalid JSON: %s", error.c_str());
    lastError = err;
        return false;
    }
    
    AutoRule& rule = autoRules[relayIndex];
    
    rule.enabled = doc["enabled"] | false;
    rule.type = doc["type"] | "";
    rule.condition = doc["condition"] | "";
    rule.value1 = doc["value1"] | 0.0;
    rule.value2 = doc["value2"] | 0.0;
    rule.schedule = doc["schedule"] | "";
    rule.duration = doc["duration"] | 0;
    // Al definir una nueva regla, reiniciar métricas (O2)
    rule.evalCount = 0;
    rule.activationCount = 0;
    rule.hourlyEvalCount = 0;
    rule.hourlyActivationCount = 0;
    rule.hourlyWindowHour = 0; // se ajustará en el próximo ciclo de evaluación
    // Reset métricas ampliadas
    rule.lastEvalAt = 0;
    rule.lastDecision = false;
    rule.currentBucket = 0;
    rule.bucketBaseMinutes = 0;
    for (int b=0;b<6;++b){ rule.evalBuckets[b]=0; rule.actBuckets[b]=0; }
    
    // Validar regla
    if (rule.type.isEmpty()) {
        lastError = "Rule type is required";
        return false;
    }
    
    if (rule.type == "schedule" && rule.schedule.isEmpty()) {
        lastError = "Schedule is required for schedule rules";
        return false;
    }
    
    if ((rule.type == "temperature" || rule.type == "humidity" || rule.type == "soil_moisture") 
        && rule.condition.isEmpty()) {
        lastError = "Condition is required for sensor rules";
        return false;
    }
    
    database.logSystemEvent("auto_rule_set", 
           "Auto rule set for relay " + relayName(relayIndex) + ": " + ruleJson);
    
    #if VERBOSE_LOGS
    DEBUG_PRINTF("Rule %s: %s\n", relayName(relayIndex).c_str(), ruleJson.c_str());
    #endif
    
    return true;
}

String RelayManager::getAutoRule(int relayIndex) {
    if (relayIndex < 0 || relayIndex >= 4) return "{}";
    
    StaticJsonDocument<512> doc;
    AutoRule& rule = autoRules[relayIndex];
    
    doc["enabled"] = rule.enabled;
    doc["type"] = rule.type;
    doc["condition"] = rule.condition;
    doc["value1"] = rule.value1;
    doc["value2"] = rule.value2;
    doc["schedule"] = rule.schedule;
    doc["duration"] = rule.duration;
    doc["lastActivation"] = rule.lastActivation;
    doc["isActive"] = rule.isActive;
    // Métricas O2
    doc["evalCount"] = rule.evalCount;
    doc["activationCount"] = rule.activationCount;
    doc["hourlyEvalCount"] = rule.hourlyEvalCount;
    doc["hourlyActivationCount"] = rule.hourlyActivationCount;
    doc["hourlyWindowHour"] = rule.hourlyWindowHour;
    
    String result;
    serializeJson(doc, result);
    return result;
}

bool RelayManager::enableAutoRule(int relayIndex, bool enable) {
    if (relayIndex < 0 || relayIndex >= 4) return false;
    
    autoRules[relayIndex].enabled = enable;
    
    database.logSystemEvent("auto_rule_toggle", 
        "Auto rule for relay " + relayName(relayIndex) + 
        (enable ? " enabled" : " disabled"));
    
    return true;
}

// Nueva implementación delegando en rule_engine
#include "rule_engine.h"

void RelayManager::processAutoRules() {
    SensorData sensorData = sensors.getCurrentData();
    if (!sensorData.valid) return;

    // Evaluate rules for each relay in auto mode using new rule engine
    for (int i = 0; i < 4; ++i) {
        if (relayStates[i].mode != RELAY_MODE_AUTO) continue;
        
        RuleAction action;
        if (ruleEngine.evaluateRules(i, sensorData, action)) {
            // Rule matched - execute action
            bool target_state = (action.type == ActionType::TURN_ON);
            
            if (target_state != relayStates[i].is_on) {
                setRelay(i, target_state);
                
                // Handle duration-based actions
                if (action.duration_min > 0 && target_state) {
                    // Schedule auto-off after duration (handled by relay timeouts)
                    // TODO: Add relay timeout mechanism for duration-based rules
                }
            }
        }
    }
}

// (Funciones de evaluación antiguas movidas a rule_engine.cpp)

void RelayManager::enforceTimeouts() {
    unsigned long currentTime = timeSource().millis();
    unsigned long lastChange[4];
    bool isOn[4];
    for (int i = 0; i < 4; ++i) { lastChange[i] = relayStates[i].last_change; isOn[i] = relayStates[i].is_on; }
    bool shouldOff[4];
    evaluateRelayTimeouts(currentTime, lastChange, isOn, shouldOff);
    for (int i = 0; i < 4; ++i) {
        if (shouldOff[i]) {
            String timeoutReason = (i == 2) ? "Irrigation timeout exceeded" : "Heating timeout exceeded";
            setRelay(i, false);
            database.logError("safety", timeoutReason + " for relay " + relayName(i));
            DEBUG_PRINTLN(String(F("SAFE OFF ")) + relayName(i));
        }
    }
}

bool RelayManager::checkSafetyLimits() {
    if (!safetyLimitsEnabled) return true;
    
    SensorData data = sensors.getCurrentData();
    if (!data.valid) return true; // Allow operation when no sensors connected
    
    // Verificar límites de temperatura
    if (data.temperature > MAX_TEMP_CELSIUS || data.temperature < MIN_TEMP_CELSIUS) {
    {
        char err[64];
        snprintf(err, sizeof(err), "Temperature out of safe range: %.1f C", data.temperature);
        lastError = err;
    }
        return false;
    }
    
    // Verificar límites de humedad
    if (data.humidity > MAX_HUMIDITY_PERCENT || data.humidity < MIN_HUMIDITY_PERCENT) {
    char err[64];
    snprintf(err, sizeof(err), "Humidity out of safe range: %.1f%%", data.humidity);
    lastError = err;
        return false;
    }
    
    return true;
}

void RelayManager::pauseSystem(bool pause) {
    if (pause != systemPaused) {
        systemPaused = pause;
        
        if (pause) {
            pauseStartTime = timeSource().millis();
            for (int i = 0; i < 4; i++) {
                if (relayStates[i].is_on) {
                    digitalWrite(relayPins[i], LOW);
                }
            }
            database.logSystemEvent("system_paused", "All relays disabled - system paused");
        } else {
            // Restaurar estados al reanudar (solo los que estaban encendidos)
            for (int i = 0; i < 4; i++) {
                if (relayStates[i].is_on) {
                    digitalWrite(relayPins[i], HIGH);
                }
            }
            {
                unsigned long secs = (timeSource().millis() - pauseStartTime) / 1000;
                char msg[64];
                snprintf(msg, sizeof(msg), "System resumed after %lus seconds", secs);
                database.logSystemEvent("system_resumed", String(msg));
            }
        }
    }
}

bool RelayManager::isSystemPaused() {
    return systemPaused;
}

void RelayManager::saveStateToNVS() {
    PersistedRelayBlock block{};
    for (int i = 0; i < 4; ++i) {
        block.entries[i].is_on = relayStates[i].is_on ? 1 : 0;
        block.entries[i].mode = (uint8_t)relayStates[i].mode;
        block.entries[i].total_on_time = relayStates[i].total_on_time;
    }
    block.systemPaused = systemPaused ? 1 : 0;
    // Load previous seq to increment
    RelayStateLoadResult prev = loadRelayStateFromFS();
    if (!saveRelayStateToFS(block, prev.seq)) {
        DEBUG_PRINTLN(F("WARN: Failed to save relay_state dual-slot; falling back to legacy NVS"));
        Preferences prefs;
        if (safePrefsBegin(prefs, "relays", false)) {
        for (int i = 0; i < 4; i++) {
            String prefix = "relay" + String(i) + "_";
            prefs.putBool((prefix + "state").c_str(), relayStates[i].is_on);
            prefs.putInt((prefix + "mode").c_str(), (int)relayStates[i].mode);
            prefs.putULong((prefix + "total_time").c_str(), relayStates[i].total_on_time);
            String ruleKey = prefix + "rule";
            prefs.putString(ruleKey.c_str(), getAutoRule(i));
        }
        prefs.putBool("system_paused", systemPaused);
        prefs.end();
        } else {
            DEBUG_PRINTLN(F("INFO: NVS not available, state not persisted"));
        }
    }
}

void RelayManager::loadStateFromNVS() {
    RelayStateLoadResult res = loadRelayStateFromFS();
    if (res.success) {
        for (int i = 0; i < 4; ++i) {
            relayStates[i].is_on = res.block.entries[i].is_on != 0;
            relayStates[i].mode = (RelayMode)res.block.entries[i].mode;
            relayStates[i].total_on_time = res.block.entries[i].total_on_time;
        }
        systemPaused = res.block.systemPaused != 0;
    } else {
        // fallback legacy - try NVS but ignore if not available
        Preferences prefs;
        if (safePrefsBegin(prefs, "relays", true)) {
            for (int i = 0; i < 4; i++) {
            String prefix = "relay" + String(i) + "_";
            relayStates[i].is_on = prefs.getBool((prefix + "state").c_str(), false);
            relayStates[i].mode = (RelayMode)prefs.getInt((prefix + "mode").c_str(), RELAY_MODE_AUTO);
            relayStates[i].total_on_time = prefs.getULong((prefix + "total_time").c_str(), 0);
            String ruleKey = prefix + "rule";
            String ruleJson = prefs.getString(ruleKey.c_str(), "{}");
            if (ruleJson != "{}") {
                setAutoRule(i, ruleJson);
            }
        }
        systemPaused = prefs.getBool("system_paused", false);
        prefs.end();
        } else {
            // NVS not available, use default values
            for (int i = 0; i < 4; i++) {
                relayStates[i].is_on = false;
                relayStates[i].mode = RELAY_MODE_AUTO;
                relayStates[i].total_on_time = 0;
            }
            systemPaused = false;
        }
    }
}

void RelayManager::restoreStateAfterPowerLoss() {
    database.logSystemEvent("state_restore", "Restoring relay states after power loss");
    
    // Restaurar estados físicos de los relays
    for (int i = 0; i < 4; i++) {
        if (relayStates[i].is_on && !systemPaused) {
            digitalWrite(relayPins[i], HIGH);
            #if VERBOSE_LOGS
            DEBUG_PRINTF("Restored %s ON\n", relayName(i).c_str());
            #endif
        } else {
            digitalWrite(relayPins[i], LOW);
        }
    }
}

String RelayManager::getSystemStatus() {
    StaticJsonDocument<1024> doc;
    
    doc["system_paused"] = systemPaused;
    doc["safety_limits_enabled"] = safetyLimitsEnabled;
    doc["uptime"] = system_manager.getUptime();
    
    JsonArray relaysArray = doc.createNestedArray("relays");
    
    for (int i = 0; i < 4; i++) {
        JsonObject relay = relaysArray.createNestedObject();
        relay["index"] = i;
    relay["name"] = relayName(i);
        relay["state"] = relayStates[i].is_on;
        relay["mode"] = (relayStates[i].mode == RELAY_MODE_MANUAL) ? "manual" : "auto";
        relay["total_on_time"] = relayStates[i].total_on_time;
        relay["last_change"] = relayStates[i].last_change;
        
        // Agregar regla automática si existe
        if (autoRules[i].enabled) {
            JsonObject ruleObj = relay.createNestedObject("auto_rule");
            ruleObj["type"] = autoRules[i].type;
            ruleObj["condition"] = autoRules[i].condition;
            ruleObj["enabled"] = autoRules[i].enabled;
            ruleObj["is_active"] = autoRules[i].isActive;
            // Parámetros base de la regla para edición en frontend
            ruleObj["value1"] = autoRules[i].value1;
            ruleObj["value2"] = autoRules[i].value2;
            if (autoRules[i].schedule.length()) ruleObj["schedule"] = autoRules[i].schedule;
            if (autoRules[i].duration) ruleObj["duration"] = autoRules[i].duration;
            ruleObj["relay_index"] = i;
            // Métricas O2
            ruleObj["eval_total"] = autoRules[i].evalCount;
            ruleObj["act_total"] = autoRules[i].activationCount;
            ruleObj["eval_hour"] = autoRules[i].hourlyEvalCount;
            ruleObj["act_hour"] = autoRules[i].hourlyActivationCount;
            ruleObj["hour"] = autoRules[i].hourlyWindowHour;
            // Métricas ampliadas (sliding 60m)
            uint32_t eval60 = 0, act60 = 0;
            for (int b=0;b<6;++b){ eval60 += autoRules[i].evalBuckets[b]; act60 += autoRules[i].actBuckets[b]; }
            ruleObj["eval_60m"] = eval60;
            ruleObj["act_60m"] = act60;
            JsonArray eb = ruleObj.createNestedArray("eval_buckets");
            JsonArray ab = ruleObj.createNestedArray("act_buckets");
            for (int b=0;b<6;++b){ eb.add(autoRules[i].evalBuckets[b]); ab.add(autoRules[i].actBuckets[b]); }
            ruleObj["bucket_minutes"] = 10;
            ruleObj["last_eval_ms"] = autoRules[i].lastEvalAt;
            ruleObj["last_decision"] = autoRules[i].lastDecision;
        }
    }
    
    String result;
    serializeJson(doc, result);
    return result;
}

SystemStats RelayManager::getRelayStatistics() {
    SystemStats stats = {0};
    
    // Calcular estadísticas de uso de relays
    for (int i = 0; i < 4; i++) {
        if (relayName(i) == "calefactor") {
            stats.heating_time = relayStates[i].total_on_time;
        } else if (relayName(i) == "bomba") {
            stats.irrigation_time = relayStates[i].total_on_time;
        }
    }
    
    stats.uptime = system_manager.getUptime();
    stats.last_reset_time = system_manager.getCurrentTimestamp();
    
    return stats;
}

void RelayManager::resetStatistics() {
    for (int i = 0; i < 4; i++) {
        relayStates[i].total_on_time = 0;
    }
    database.logSystemEvent("stats_reset", "Relay statistics reset");
}