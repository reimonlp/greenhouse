// Pure helper for relay timeout evaluation (testable in native env)
#ifndef RELAY_TIMEOUTS_H
#define RELAY_TIMEOUTS_H

#include <stddef.h>
#include <stdint.h>
#include "config.h"

// Relay indices for clarity
enum RelayIndex { RELAY_IDX_LUCES = 0, RELAY_IDX_VENTILADOR = 1, RELAY_IDX_BOMBA = 2, RELAY_IDX_CALEFACTOR = 3 };

// Evaluate which relays should be turned off due to safety timeouts.
// Inputs:
//   now               - current time (ms)
//   lastChange[4]     - timestamp when relay was last turned ON
//   isOn[4]           - current on/off states
// Output:
//   shouldOff[4]      - set to true for relays that exceed timeout (others false)
// Logic replicates RelayManager::enforceTimeouts() semantics (> threshold, not >=).
void evaluateRelayTimeouts(unsigned long now,
                           const unsigned long lastChange[4],
                           const bool isOn[4],
                           bool shouldOff[4]);

#endif // RELAY_TIMEOUTS_H
