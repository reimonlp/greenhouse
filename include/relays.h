#ifndef RELAYS_H
#define RELAYS_H

#include "config.h"

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
    
    bool begin();
    void update();
    bool setRelay(int relayIndex, bool state);
    bool toggleRelay(int relayIndex);
    bool getRelayState(int relayIndex);
    RelayState getRelayStateStruct(int relayIndex);
    String relayName(uint8_t idx) const;
    
    static const uint8_t relayPins[4];
};

extern RelayManager relays;

#endif
