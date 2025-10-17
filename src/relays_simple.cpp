// Simplified RelayManager for VPS client mode
// No rule engine, no persistence, just basic relay control

#include "relays.h"
#include <Arduino.h>

// Define relay pins array
const uint8_t RelayManager::relayPins[4] = {
    RELAY_LUCES_PIN,
    RELAY_VENTILADOR_PIN,
    RELAY_BOMBA_PIN,
    RELAY_CALEFACTOR_PIN
};

// Static strings in PROGMEM for relay names
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

// Global instance
RelayManager relays;

RelayManager::RelayManager() : lastAutoCheck(0), safetyLimitsEnabled(false) {
    for (int i = 0; i < 4; i++) {
        relayStates[i].is_on = false;
        relayStates[i].mode = RELAY_MODE_MANUAL;
        relayStates[i].last_change = 0;
        relayStates[i].total_on_time = 0;
        relayStates[i].auto_rule = "";
    }
}

String RelayManager::relayName(uint8_t idx) const {
    if (idx >= 4) return "unknown";
    char buffer[32];
    strcpy_P(buffer, (char*)pgm_read_ptr(&relayNames_P[idx]));
    return String(buffer);
}

bool RelayManager::begin() {
    DEBUG_PRINTLN("Initializing relays...");
    
    for (int i = 0; i < 4; i++) {
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], LOW);  // Relays OFF by default (active HIGH)
        relayStates[i].is_on = false;
        DEBUG_PRINTF("  Relay %d (%s): PIN %d - OFF\n", i, relayName(i).c_str(), relayPins[i]);
    }
    
    DEBUG_PRINTLN("[OK] Relays initialized");
    return true;
}

void RelayManager::update() {
    // No-op in VPS client mode
    // Relay states are controlled from VPS
}

bool RelayManager::setRelay(int relayIndex, bool state) {
    if (relayIndex < 0 || relayIndex >= 4) {
        DEBUG_PRINTF("Invalid relay index: %d\n", relayIndex);
        return false;
    }
    
    digitalWrite(relayPins[relayIndex], state ? HIGH : LOW);
    relayStates[relayIndex].is_on = state;
    relayStates[relayIndex].last_change = millis();
    relayStates[relayIndex].mode = RELAY_MODE_MANUAL;
    
    DEBUG_PRINTF("Relay %d (%s): %s\n", relayIndex, relayName(relayIndex).c_str(), state ? "ON" : "OFF");
    
    return true;
}

bool RelayManager::toggleRelay(int relayIndex) {
    if (relayIndex < 0 || relayIndex >= 4) {
        return false;
    }
    
    return setRelay(relayIndex, !relayStates[relayIndex].is_on);
}

bool RelayManager::getRelayState(int relayIndex) {
    if (relayIndex < 0 || relayIndex >= 4) {
        return false;
    }
    
    return relayStates[relayIndex].is_on;
}

RelayState RelayManager::getRelayStateStruct(int relayIndex) {
    if (relayIndex < 0 || relayIndex >= 4) {
        RelayState empty;
        empty.is_on = false;
        empty.mode = RELAY_MODE_MANUAL;
        empty.last_change = 0;
        empty.total_on_time = 0;
        empty.auto_rule = "";
        return empty;
    }
    
    return relayStates[relayIndex];
}
