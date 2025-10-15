/**
 * Rule Engine Implementation
 */

#include "rule_engine.h"
#include "config.h"
#include "relays.h"
#include "time_source.h"
#include <LittleFS.h>
#include <time.h>

// Global instance
RuleEngine ruleEngine;

// ============================================
// RuleCondition Implementation
// ============================================

bool RuleCondition::evaluate(const SensorData& sensors, uint8_t current_relay_index) const {
    switch (type) {
        case ConditionType::TIME_RANGE: {
            time_t now = time(nullptr);
            struct tm timeinfo;
            if (!localtime_r(&now, &timeinfo)) return false;
            
            int current_minutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
            int start_minutes = start_hour * 60 + start_minute;
            int end_minutes = end_hour * 60 + end_minute;
            
            if (start_minutes <= end_minutes) {
                return current_minutes >= start_minutes && current_minutes <= end_minutes;
            } else {
                return current_minutes >= start_minutes || current_minutes <= end_minutes;
            }
        }
        
        case ConditionType::WEEKDAY: {
            time_t now = time(nullptr);
            struct tm timeinfo;
            if (!localtime_r(&now, &timeinfo)) return false;
            
            uint8_t weekday = timeinfo.tm_wday;
            return (weekday_mask & (1 << weekday)) != 0;
        }
        
        case ConditionType::SENSOR: {
            if (!sensors.valid) return false;
            
            float sensor_val = 0.0;
            if (sensor_type == "temperature") {
                sensor_val = sensors.temperature;
            } else if (sensor_type == "humidity") {
                sensor_val = sensors.humidity;
            } else if (sensor_type == "soil1") {
                sensor_val = sensors.soil_moisture_1;
            } else if (sensor_type == "soil2") {
                sensor_val = sensors.soil_moisture_2;
            } else {
                return false;
            }
            
            switch (sensor_op) {
                case SensorOp::GT:
                    return sensor_val > sensor_value;
                case SensorOp::LT:
                    return sensor_val < sensor_value;
                case SensorOp::EQ:
                    return abs(sensor_val - sensor_value) < 0.01;
                case SensorOp::BETWEEN:
                    return sensor_val >= sensor_value && sensor_val <= sensor_value2;
                default:
                    return false;
            }
        }
        
        case ConditionType::RELAY_STATE: {
            if (relay_index >= 4) return false;
            
            RelayState state = relays.getRelayStateStruct(relay_index);
            unsigned long elapsed_min = (millis() - state.last_change) / 60000;
            
            switch (relay_op) {
                case RelayStateOp::IS_ON:
                    return state.is_on;
                case RelayStateOp::IS_OFF:
                    return !state.is_on;
                case RelayStateOp::ON_DURATION:
                    return state.is_on && elapsed_min >= duration_min;
                case RelayStateOp::OFF_DURATION:
                    return !state.is_on && elapsed_min >= duration_min;
                default:
                    return false;
            }
        }
        
        default:
            return false;
    }
}

bool RuleCondition::toJson(JsonObject& obj) const {
    switch (type) {
        case ConditionType::TIME_RANGE:
            obj["type"] = "time_range";
            obj["start"] = String(start_hour) + ":" + (start_minute < 10 ? "0" : "") + String(start_minute);
            obj["end"] = String(end_hour) + ":" + (end_minute < 10 ? "0" : "") + String(end_minute);
            break;
            
        case ConditionType::WEEKDAY: {
            obj["type"] = "weekday";
            JsonArray days = obj.createNestedArray("days");
            for (uint8_t i = 0; i < 7; i++) {
                if (weekday_mask & (1 << i)) {
                    days.add(i);
                }
            }
            break;
        }
        
        case ConditionType::SENSOR:
            obj["type"] = "sensor";
            obj["sensor"] = sensor_type;
            obj["op"] = (sensor_op == SensorOp::GT ? "gt" : 
                        (sensor_op == SensorOp::LT ? "lt" : 
                        (sensor_op == SensorOp::EQ ? "eq" : "between")));
            obj["value"] = sensor_value;
            if (sensor_op == SensorOp::BETWEEN) {
                obj["value2"] = sensor_value2;
            }
            break;
            
        case ConditionType::RELAY_STATE:
            obj["type"] = "relay_state";
            obj["relay_index"] = relay_index;
            obj["state_op"] = (relay_op == RelayStateOp::IS_ON ? "on" :
                              (relay_op == RelayStateOp::IS_OFF ? "off" :
                              (relay_op == RelayStateOp::ON_DURATION ? "on_duration" : "off_duration")));
            if (relay_op == RelayStateOp::ON_DURATION || relay_op == RelayStateOp::OFF_DURATION) {
                obj["duration_min"] = duration_min;
            }
            break;
            
        default:
            return false;
    }
    return true;
}

bool RuleCondition::fromJson(const JsonObject& obj) {
    String type_str = obj["type"].as<String>();
    
    if (type_str == "time_range") {
        type = ConditionType::TIME_RANGE;
        String start = obj["start"].as<String>();
        String end = obj["end"].as<String>();
        sscanf(start.c_str(), "%hhu:%hhu", &start_hour, &start_minute);
        sscanf(end.c_str(), "%hhu:%hhu", &end_hour, &end_minute);
        
    } else if (type_str == "weekday") {
        type = ConditionType::WEEKDAY;
        weekday_mask = 0;
        JsonArray days = obj["days"];
        for (JsonVariant day : days) {
            uint8_t d = day.as<uint8_t>();
            if (d < 7) weekday_mask |= (1 << d);
        }
        
    } else if (type_str == "sensor") {
        type = ConditionType::SENSOR;
        sensor_type = obj["sensor"].as<String>();
        String op = obj["op"].as<String>();
        sensor_op = (op == "gt" ? SensorOp::GT :
                    (op == "lt" ? SensorOp::LT :
                    (op == "eq" ? SensorOp::EQ : SensorOp::BETWEEN)));
        sensor_value = obj["value"];
        if (sensor_op == SensorOp::BETWEEN) {
            sensor_value2 = obj["value2"];
        }
        
    } else if (type_str == "relay_state") {
        type = ConditionType::RELAY_STATE;
        relay_index = obj["relay_index"];
        String op = obj["state_op"].as<String>();
        relay_op = (op == "on" ? RelayStateOp::IS_ON :
                   (op == "off" ? RelayStateOp::IS_OFF :
                   (op == "on_duration" ? RelayStateOp::ON_DURATION : RelayStateOp::OFF_DURATION)));
        if (relay_op == RelayStateOp::ON_DURATION || relay_op == RelayStateOp::OFF_DURATION) {
            duration_min = obj["duration_min"];
        }
        
    } else {
        return false;
    }
    
    return true;
}

// ============================================
// RuleAction Implementation
// ============================================

bool RuleAction::toJson(JsonObject& obj) const {
    obj["type"] = (type == ActionType::TURN_ON ? "turn_on" : "turn_off");
    if (duration_min > 0) {
        obj["duration_min"] = duration_min;
    }
    return true;
}

bool RuleAction::fromJson(const JsonObject& obj) {
    String type_str = obj["type"].as<String>();
    type = (type_str == "turn_on" ? ActionType::TURN_ON : ActionType::TURN_OFF);
    duration_min = obj["duration_min"] | 0;
    return true;
}

// ============================================
// Rule Implementation
// ============================================

bool Rule::evaluate(const SensorData& sensors, uint8_t relay_index) const {
    if (!enabled) return false;
    
    for (const auto& condition : conditions) {
        if (!condition.evaluate(sensors, relay_index)) {
            return false;
        }
    }
    
    return true;
}

bool Rule::canActivate() const {
    if (!enabled) return false;
    if (!repeat && has_run_once) return false;
    
    if (cooldown_min > 0 && last_activation_ms > 0) {
        unsigned long elapsed_min = (millis() - last_activation_ms) / 60000;
        if (elapsed_min < cooldown_min) return false;
    }
    
    return true;
}

void Rule::markActivated() {
    last_activation_ms = millis();
    has_run_once = true;
}

bool Rule::toJson(JsonObject& obj) const {
    obj["name"] = name;
    obj["enabled"] = enabled;
    obj["priority"] = priority;
    
    JsonArray conds = obj.createNestedArray("conditions");
    for (const auto& cond : conditions) {
        JsonObject condObj = conds.createNestedObject();
        cond.toJson(condObj);
    }
    
    JsonObject actObj = obj.createNestedObject("action");
    action.toJson(actObj);
    
    obj["cooldown_min"] = cooldown_min;
    obj["repeat"] = repeat;
    
    return true;
}

bool Rule::fromJson(const JsonObject& obj) {
    name = obj["name"].as<String>();
    enabled = obj["enabled"] | true;
    priority = obj["priority"] | 5;
    
    conditions.clear();
    JsonArray conds = obj["conditions"];
    for (JsonVariant condVar : conds) {
        RuleCondition cond;
        if (cond.fromJson(condVar.as<JsonObject>())) {
            conditions.push_back(cond);
        }
    }
    
    action.fromJson(obj["action"]);
    
    cooldown_min = obj["cooldown_min"] | 0;
    repeat = obj["repeat"] | true;
    
    return true;
}

// ============================================
// RuleEngine Implementation
// ============================================

RuleEngine::RuleEngine() {
}

bool RuleEngine::begin() {
    for (uint8_t i = 0; i < 4; i++) {
        loadRules(i);
    }
    return true;
}

bool RuleEngine::loadRules(uint8_t relay_index) {
    if (relay_index >= 4) return false;
    return loadRulesFromFile(relay_index, getRulesFilePath(relay_index));
}

bool RuleEngine::saveRules(uint8_t relay_index) {
    if (relay_index >= 4) return false;
    return saveRulesToFile(relay_index, getRulesFilePath(relay_index));
}

bool RuleEngine::addRule(uint8_t relay_index, const Rule& rule) {
    if (relay_index >= 4) return false;
    rules_[relay_index].push_back(rule);
    return saveRules(relay_index);
}

bool RuleEngine::updateRule(uint8_t relay_index, size_t rule_index, const Rule& rule) {
    if (relay_index >= 4 || rule_index >= rules_[relay_index].size()) return false;
    rules_[relay_index][rule_index] = rule;
    return saveRules(relay_index);
}

bool RuleEngine::deleteRule(uint8_t relay_index, size_t rule_index) {
    if (relay_index >= 4 || rule_index >= rules_[relay_index].size()) return false;
    rules_[relay_index].erase(rules_[relay_index].begin() + rule_index);
    return saveRules(relay_index);
}

bool RuleEngine::clearRules(uint8_t relay_index) {
    if (relay_index >= 4) return false;
    rules_[relay_index].clear();
    return saveRules(relay_index);
}

const std::vector<Rule>& RuleEngine::getRules(uint8_t relay_index) const {
    static std::vector<Rule> empty;
    if (relay_index >= 4) return empty;
    return rules_[relay_index];
}

size_t RuleEngine::getRuleCount(uint8_t relay_index) const {
    if (relay_index >= 4) return 0;
    return rules_[relay_index].size();
}

const Rule* RuleEngine::getRule(uint8_t relay_index, size_t rule_index) const {
    if (relay_index >= 4 || rule_index >= rules_[relay_index].size()) return nullptr;
    return &rules_[relay_index][rule_index];
}

bool RuleEngine::evaluateRules(uint8_t relay_index, const SensorData& sensors, RuleAction& out_action) {
    if (relay_index >= 4) return false;
    
    const Rule* best_rule = nullptr;
    int8_t best_priority = -1;
    
    for (auto& rule : rules_[relay_index]) {
        if (!rule.canActivate()) continue;
        if (!rule.evaluate(sensors, relay_index)) continue;
        
        if (rule.priority > best_priority) {
            best_rule = &rule;
            best_priority = rule.priority;
        }
    }
    
    if (best_rule) {
        out_action = best_rule->action;
        for (auto& rule : rules_[relay_index]) {
            if (&rule == best_rule) {
                rule.markActivated();
                break;
            }
        }
        return true;
    }
    
    return false;
}

String RuleEngine::exportRules(uint8_t relay_index) const {
    if (relay_index >= 4) return "[]";
    
    DynamicJsonDocument doc(8192);
    JsonArray arr = doc.to<JsonArray>();
    
    for (const auto& rule : rules_[relay_index]) {
        JsonObject ruleObj = arr.createNestedObject();
        rule.toJson(ruleObj);
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

bool RuleEngine::importRules(uint8_t relay_index, const String& json) {
    if (relay_index >= 4) return false;
    
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, json);
    if (error) return false;
    
    rules_[relay_index].clear();
    JsonArray arr = doc.as<JsonArray>();
    
    for (JsonVariant ruleVar : arr) {
        Rule rule;
        if (rule.fromJson(ruleVar.as<JsonObject>())) {
            rules_[relay_index].push_back(rule);
        }
    }
    
    return saveRules(relay_index);
}

String RuleEngine::getRulesFilePath(uint8_t relay_index) const {
    return String("/rules_relay_") + String(relay_index) + ".json";
}

bool RuleEngine::loadRulesFromFile(uint8_t relay_index, const String& path) {
    if (!LittleFS.exists(path)) {
        rules_[relay_index].clear();
        return true;
    }
    
    File file = LittleFS.open(path, "r");
    if (!file) return false;
    
    String json = file.readString();
    file.close();
    
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, json);
    if (error) return false;
    
    rules_[relay_index].clear();
    JsonArray arr = doc.as<JsonArray>();
    
    for (JsonVariant ruleVar : arr) {
        Rule rule;
        if (rule.fromJson(ruleVar.as<JsonObject>())) {
            rules_[relay_index].push_back(rule);
        }
    }
    
    return true;
}

bool RuleEngine::saveRulesToFile(uint8_t relay_index, const String& path) {
    DynamicJsonDocument doc(8192);
    JsonArray arr = doc.to<JsonArray>();
    
    for (const auto& rule : rules_[relay_index]) {
        JsonObject ruleObj = arr.createNestedObject();
        rule.toJson(ruleObj);
    }
    
    File file = LittleFS.open(path, "w", true);
    if (!file) return false;
    
    size_t bytesWritten = serializeJson(doc, file);
    file.close();
    
    return bytesWritten > 0;
}

