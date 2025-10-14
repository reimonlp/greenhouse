// relay_state_store.h - Dual-slot persisted relay state with checksum for power loss resilience
#ifndef RELAY_STATE_STORE_H
#define RELAY_STATE_STORE_H

#include <stdint.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

struct PersistedRelayEntry {
    uint8_t is_on;      // 0/1
    uint8_t mode;       // RelayMode enum value
    uint16_t reserved;  // alignment / future
    uint32_t total_on_time; // ms accumulated
};

struct PersistedRelayBlock {
    PersistedRelayEntry entries[4];
    uint8_t systemPaused; // 0/1
    uint8_t reserved[3];
};

// Result of load attempt
struct RelayStateLoadResult {
    bool success;          // true if a valid slot was found
    PersistedRelayBlock block; // filled if success
    uint32_t seq;          // sequence of chosen slot
};

// Build a binary buffer (excluding CRC) and compute CRC32.
size_t encodeRelayState(const PersistedRelayBlock& block, uint32_t seq, uint8_t* outBuf, size_t maxLen, uint32_t& outCrc);
// Parse buffer (including CRC at end). Returns true if valid and fills block / seq.
bool decodeRelayState(const uint8_t* data, size_t len, PersistedRelayBlock& block, uint32_t& seq);

#ifdef ARDUINO
// Load from dual slots in LittleFS.
RelayStateLoadResult loadRelayStateFromFS();
// Save (encode + write) selecting the alternate slot, returns true on success.
bool saveRelayStateToFS(const PersistedRelayBlock& block, uint32_t previousSeq);
#endif

#endif // RELAY_STATE_STORE_H
