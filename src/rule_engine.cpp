// Standalone rule engine implementation (no Arduino dependencies) 
#include "rule_engine.h"
#include <algorithm>

static int toMinutes(int hour, int minute) { return hour * 60 + minute; }

bool evaluateSchedule(const std::string& range, int hour, int minute) {
    if (range.size() < 11) return false; // minimal "HH:MM-HH:MM"
    auto dash = range.find('-');
    if (dash == std::string::npos) return false;
    auto start = range.substr(0, dash);
    auto end = range.substr(dash + 1);
    if (start.size() != 5 || end.size() != 5) return false;
    int sh = std::stoi(start.substr(0,2));
    int sm = std::stoi(start.substr(3,2));
    int eh = std::stoi(end.substr(0,2));
    int em = std::stoi(end.substr(3,2));
    int startM = toMinutes(sh, sm);
    int endM = toMinutes(eh, em);
    int curM = toMinutes(hour, minute);
    if (endM < startM) { // crosses midnight
        return curM >= startM || curM <= endM;
    }
    return curM >= startM && curM <= endM;
}

bool evaluateComparator(const std::string& condition, float value, float v1, float v2) {
    if (condition == "greater_than") return value > v1;
    if (condition == "less_than") return value < v1;
    if (condition == "between") return value >= v1 && value <= v2;
    return false;
}

static bool evaluateTimer(const RuleDefinition& rule, uint32_t now, bool currentlyOn) {
    if (rule.duration == 0) return false;
    if (rule.lastActivation == 0) return true; // first activation
    if (rule.isActive) {
        return (now - rule.lastActivation) < rule.duration; // remain ON until duration
    } else {
        uint32_t interval = rule.duration * 2; // simple ON duration + OFF duration symmetry
        return (now - rule.lastActivation) >= interval;
    }
}

bool evaluateRule(const RuleDefinition& rule, const RuleEvalContext& ctx, bool currentlyOn) {
    if (!rule.enabled) return false;
    if (rule.type == "schedule") {
        return evaluateSchedule(rule.schedule, ctx.currentHour, ctx.currentMinute);
    } else if (rule.type == "temperature") {
        return evaluateComparator(rule.condition, ctx.temperature, rule.value1, rule.value2);
    } else if (rule.type == "humidity") {
        return evaluateComparator(rule.condition, ctx.humidity, rule.value1, rule.value2);
    } else if (rule.type == "soil_moisture") {
        return evaluateComparator(rule.condition, ctx.soilMoistureAvg, rule.value1, rule.value2);
    } else if (rule.type == "timer") {
        return evaluateTimer(rule, ctx.millisNow, currentlyOn);
    }
    return false;
}
