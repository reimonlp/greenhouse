#include "relay_timeouts.h"

void evaluateRelayTimeouts(unsigned long now,
                           const unsigned long lastChange[4],
                           const bool isOn[4],
                           bool shouldOff[4]) {
    for (int i = 0; i < 4; ++i) {
        shouldOff[i] = false;
        if (!isOn[i]) continue;
        unsigned long onTime = now - lastChange[i];
        if (i == RELAY_IDX_BOMBA) {
            if (onTime > MAX_IRRIGATION_TIME_MS) shouldOff[i] = true;
        } else if (i == RELAY_IDX_CALEFACTOR) {
            if (onTime > MAX_HEATING_TIME_MS) shouldOff[i] = true;
        }
    }
}
