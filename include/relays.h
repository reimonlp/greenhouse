#ifndef RELAYS_H
#define RELAYS_H

#include "config.h"

/**
 * @class RelayManager
 * @brief Controls 4-channel relay board for greenhouse actuators
 * 
 * Manages relay states, safety limits, and automated control for:
 * - Lights (relay 0)
 * - Ventilation fan (relay 1) 
 * - Water pump (relay 2)
 * - Heating element (relay 3)
 * 
 * Features:
 * - PROGMEM storage for relay names (saves RAM)
 * - Safety timeout protection
 * - State persistence and recovery
 * - Automated control based on sensor thresholds
 */
class RelayManager {
private:
    RelayState relayStates[4];
    unsigned long lastAutoCheck;
    bool safetyLimitsEnabled;
    
    static const char relayName0[] PROGMEM;
    static const char relayName1[] PROGMEM;
    static const char relayName2[] PROGMEM;
    static const char relayName3[] PROGMEM;
    static const char* const relayNames_P[4] PROGMEM;

public:
    RelayManager();
    
    /**
     * @brief Initialize relay pins and load saved states
     * @return true if initialization successful
     */
    bool begin();
    
    /**
     * @brief Update relay states based on automation rules
     * Called periodically from main loop
     */
    void update();
    
    /**
     * @brief Set specific relay to desired state
     * @param relayIndex Relay number (0-3)
     * @param state true=on, false=off
     * @return true if relay set successfully
     */
    bool setRelay(int relayIndex, bool state);
    
    /**
     * @brief Toggle relay state (on->off, off->on)
     * @param relayIndex Relay number (0-3)
     * @return true if toggle successful
     */
    bool toggleRelay(int relayIndex);
    
    /**
     * @brief Get current relay state
     * @param relayIndex Relay number (0-3)
     * @return true if relay is on
     */
    bool getRelayState(int relayIndex);
    
    /**
     * @brief Get complete relay state information
     * @param relayIndex Relay number (0-3)
     * @return RelayState struct with full state info
     */
    RelayState getRelayStateStruct(int relayIndex);
    
    /**
     * @brief Get human-readable relay name
     * @param idx Relay index (0-3)
     * @return String with relay description
     */
    String relayName(uint8_t idx) const;
    
    static const uint8_t relayPins[4];
};

extern RelayManager relays;

#endif
