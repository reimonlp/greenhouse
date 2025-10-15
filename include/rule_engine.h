/**
 * Rule Engine - Automatic relay control based on configurable rules
 */

#ifndef RULE_ENGINE_H
#define RULE_ENGINE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

// Forward declarations
struct SensorData;

// ============================================
// Rule Condition Types
// ============================================

enum class ConditionType {
    TIME_RANGE,      // Hour range (e.g., 08:00-18:00)
    WEEKDAY,         // Days of week (0=Sunday, 6=Saturday)
    SENSOR,          // Sensor threshold (temp, humidity, soil)
    RELAY_STATE      // Other relay state/duration
};

enum class SensorOp {
    GT,              // Greater than
    LT,              // Less than
    EQ,              // Equal
    BETWEEN          // Between two values
};

enum class RelayStateOp {
    IS_ON,           // Relay is ON
    IS_OFF,          // Relay is OFF
    ON_DURATION,     // Relay has been ON for X minutes
    OFF_DURATION     // Relay has been OFF for X minutes
};

struct RuleCondition {
    ConditionType type;
    
    // TIME_RANGE
    uint8_t start_hour = 0;
    uint8_t start_minute = 0;
    uint8_t end_hour = 23;
    uint8_t end_minute = 59;
    
    // WEEKDAY (bitfield: bit 0 = Sunday, bit 6 = Saturday)
    uint8_t weekday_mask = 0x7F;  // Default: all days
    
    // SENSOR
    String sensor_type;           // "temperature", "humidity", "soil1", "soil2"
    SensorOp sensor_op = SensorOp::GT;
    float sensor_value = 0.0;
    float sensor_value2 = 0.0;    // For BETWEEN operator
    
    // RELAY_STATE
    uint8_t relay_index = 0;
    RelayStateOp relay_op = RelayStateOp::IS_ON;
    uint16_t duration_min = 0;
    
    bool evaluate(const SensorData& sensors, uint8_t current_relay_index) const;
    bool toJson(JsonObject& obj) const;
    bool fromJson(const JsonObject& obj);
};

// ============================================
// Rule Action
// ============================================

enum class ActionType {
    TURN_ON,
    TURN_OFF
};

struct RuleAction {
    ActionType type = ActionType::TURN_ON;
    uint16_t duration_min = 0;  // 0 = indefinite
    
    bool toJson(JsonObject& obj) const;
    bool fromJson(const JsonObject& obj);
};

// ============================================
// Rule
// ============================================

struct Rule {
    String name;
    bool enabled = true;
    uint8_t priority = 5;         // 0-10, higher = more priority
    std::vector<RuleCondition> conditions;  // All must be true (AND logic)
    RuleAction action;
    uint16_t cooldown_min = 0;    // Minimum time between activations
    bool repeat = true;           // If false, rule runs only once per session
    
    // Runtime state
    unsigned long last_activation_ms = 0;
    bool has_run_once = false;
    
    bool evaluate(const SensorData& sensors, uint8_t relay_index) const;
    bool canActivate() const;
    void markActivated();
    
    bool toJson(JsonObject& obj) const;
    bool fromJson(const JsonObject& obj);
};

// ============================================
// Rule Engine
// ============================================

class RuleEngine {
public:
    RuleEngine();
    
    // Initialization
    bool begin();
    
    // Rule management
    bool loadRules(uint8_t relay_index);
    bool saveRules(uint8_t relay_index);
    bool addRule(uint8_t relay_index, const Rule& rule);
    bool updateRule(uint8_t relay_index, size_t rule_index, const Rule& rule);
    bool deleteRule(uint8_t relay_index, size_t rule_index);
    bool clearRules(uint8_t relay_index);
    
    // Rule access
    const std::vector<Rule>& getRules(uint8_t relay_index) const;
    size_t getRuleCount(uint8_t relay_index) const;
    const Rule* getRule(uint8_t relay_index, size_t rule_index) const;
    
    // Rule evaluation
    bool evaluateRules(uint8_t relay_index, const SensorData& sensors, RuleAction& out_action);
    
    // Export/Import
    String exportRules(uint8_t relay_index) const;
    bool importRules(uint8_t relay_index, const String& json);
    
private:
    std::vector<Rule> rules_[4];  // One vector per relay (0-3)
    
    String getRulesFilePath(uint8_t relay_index) const;
    bool loadRulesFromFile(uint8_t relay_index, const String& path);
    bool saveRulesToFile(uint8_t relay_index, const String& path);
};

// Global instance
extern RuleEngine ruleEngine;

#endif // RULE_ENGINE_H
