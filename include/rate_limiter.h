// rate_limiter.h
// Simple, pure IP-based sliding window rate limiter extracted from api.cpp logic.
// Template over slot count to keep static storage (suitable for embedded) while
// allowing deterministic unit tests in the native environment.

#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <stdint.h>
#include <array>

struct RateLimiterEntry {
    uint32_t ip;          // IPv4 as 32-bit integer
    uint32_t windowStart; // Start timestamp (ms) of the active window
    uint16_t count;       // Number of requests in current window
    bool active;          // Slot in use
};

template <size_t SLOTS>
class RateLimiter {
public:
    RateLimiter(uint32_t windowMs, uint16_t maxRequests)
        : _windowMs(windowMs), _maxRequests(maxRequests), _evictions(0) {
        for (auto &e : _table) { e.active = false; e.count = 0; e.windowStart = 0; e.ip = 0; }
    }

    // Returns true if the request should be allowed, false if blocked.
    // If firstExceed is provided it is set to true exactly on the first request
    // that crosses the threshold for the current window for that IP.
    bool allow(uint32_t ip, uint32_t nowMs, bool *firstExceed = nullptr) {
        if (firstExceed) *firstExceed = false;

        int freeIndex = -1;
        for (size_t i = 0; i < SLOTS; ++i) {
            auto &e = _table[i];
            if (e.active) {
                if (e.ip == ip) {
                    if (nowMs - e.windowStart >= _windowMs) {
                        // New window
                        e.windowStart = nowMs;
                        e.count = 1;
                        return true;
                    }
                    e.count++;
                    if (e.count > _maxRequests) {
                        if (e.count == (uint16_t)(_maxRequests + 1) && firstExceed)
                            *firstExceed = true;
                        return false;
                    }
                    return true;
                }
            } else if (freeIndex == -1) {
                freeIndex = (int)i;
            }
        }

        if (freeIndex != -1) {
            auto &e = _table[freeIndex];
            e.active = true;
            e.ip = ip;
            e.count = 1;
            e.windowStart = nowMs;
            return true;
        }

        // Evict oldest window (simple policy adequate for small table)
        size_t oldest = 0;
        uint32_t oldestStart = _table[0].windowStart;
        for (size_t i = 1; i < SLOTS; ++i) {
            if (_table[i].windowStart < oldestStart) {
                oldest = i;
                oldestStart = _table[i].windowStart;
            }
        }
        auto &e = _table[oldest];
        e.active = true;
        e.ip = ip;
        e.count = 1;
        e.windowStart = nowMs;
        _evictions++;
        return true;
    }

    struct SnapshotEntry { uint32_t ip; uint16_t count; uint32_t windowStart; };
    struct Snapshot { uint32_t windowMs; uint16_t maxRequests; uint16_t active; uint32_t evictions; SnapshotEntry entries[SLOTS]; };
    void snapshot(Snapshot &out) const {
        out.windowMs = _windowMs; out.maxRequests = _maxRequests; out.evictions = _evictions; out.active = 0;
        for (size_t i=0;i<SLOTS;++i){
            if (_table[i].active){
                out.entries[out.active].ip = _table[i].ip;
                out.entries[out.active].count = _table[i].count;
                out.entries[out.active].windowStart = _table[i].windowStart;
                out.active++;
            }
        }
    }

    uint32_t getEvictions() const { return _evictions; }
    uint16_t slotCapacity() const { return (uint16_t)SLOTS; }
    uint32_t windowMs() const { return _windowMs; }
    uint16_t maxRequests() const { return _maxRequests; }

private:
    std::array<RateLimiterEntry, SLOTS> _table;
    uint32_t _windowMs;
    uint16_t _maxRequests;
    uint32_t _evictions;
};

#endif // RATE_LIMITER_H
