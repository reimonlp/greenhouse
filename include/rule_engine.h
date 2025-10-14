// Rule engine extracted from RelayManager for standalone testing (native + firmware)
#ifndef RULE_ENGINE_H
#define RULE_ENGINE_H

#include <string>
#include <cstdint>

struct RuleDefinition {
    bool enabled = false;
    std::string type;        // schedule, temperature, humidity, soil_moisture, timer
    std::string condition;   // greater_than, less_than, between, time_range
    float value1 = 0.0f;
    float value2 = 0.0f;
    std::string schedule;    // HH:MM-HH:MM
    uint32_t duration = 0;   // For timer rule (ms)
    uint32_t lastActivation = 0; // millis of last ON
    bool isActive = false;   // Whether currently active (for timer/stateful rules)
};

struct RuleEvalContext {
    float temperature = 0.0f;
    float humidity = 0.0f;
    float soilMoistureAvg = 0.0f;
    uint32_t millisNow = 0;      // Current millis
    int currentHour = 0;         // Local hour (0-23)
    int currentMinute = 0;       // Local minute (0-59)
};

// Evaluate a single rule (stateless except for timer fields) and return whether
// the relay SHOULD be ON right now. Caller updates lastActivation/isActive when turning ON/OFF.
bool evaluateRule(const RuleDefinition& rule, const RuleEvalContext& ctx, bool currentlyOn);

// Helpers exposed for unit tests
bool evaluateSchedule(const std::string& range, int hour, int minute);
bool evaluateComparator(const std::string& condition, float value, float v1, float v2);

#endif // RULE_ENGINE_H
