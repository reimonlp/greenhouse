// relay_serialization.h
// Minimal, dependency-light JSON (subset) serialization helpers for relay auto rules.
// Designed to be testable under native platform without pulling full ArduinoJson
// to keep native test builds lean. NOT a general JSON parser; fits the controlled
// shape produced by serializeAutoRule.

#ifndef RELAY_SERIALIZATION_H
#define RELAY_SERIALIZATION_H

#include <string>
#include <sstream>
#include <cctype>
#include <stdint.h>

struct PlainAutoRule {
    bool enabled = false;
    std::string type;          // "schedule" | "timer" | etc.
    std::string condition;     // optional comparator string
    float value1 = 0.0f;
    float value2 = 0.0f;
    std::string schedule;      // e.g. "06:00-08:00"
    uint32_t duration = 0;     // for timer rules
    bool isActive = false;     // runtime active state
    uint32_t lastActivation = 0; // ms timestamp
};

inline static std::string boolToStr(bool b) { return b ? "true" : "false"; }

inline std::string escapeJson(const std::string &in) {
    std::string out; out.reserve(in.size()+4);
    for(char c: in){ if(c=='"' || c=='\\') out.push_back('\\'); out.push_back(c);} return out;
}

inline std::string serializeAutoRule(const PlainAutoRule &r) {
    std::ostringstream oss;
    oss << '{';
    oss << "\"enabled\":" << boolToStr(r.enabled);
    oss << ",\"type\":\"" << escapeJson(r.type) << "\"";
    if (!r.condition.empty()) oss << ",\"condition\":\"" << escapeJson(r.condition) << "\"";
    if (r.value1 != 0.0f) oss << ",\"value1\":" << r.value1;
    if (r.value2 != 0.0f) oss << ",\"value2\":" << r.value2;
    if (!r.schedule.empty()) oss << ",\"schedule\":\"" << escapeJson(r.schedule) << "\"";
    if (r.duration != 0) oss << ",\"duration\":" << r.duration;
    if (r.lastActivation != 0) oss << ",\"lastActivation\":" << r.lastActivation;
    oss << ",\"isActive\":" << boolToStr(r.isActive);
    oss << '}';
    return oss.str();
}

// Very small helpers to extract primitive fields from the known JSON layout.
inline bool extractString(const std::string &json, const std::string &key, std::string &out) {
    std::string pat = "\"" + key + "\":"; auto pos = json.find(pat); if(pos==std::string::npos) return false;
    pos += pat.size(); if(pos>=json.size()) return false; if(json[pos] != '"') return false; pos++;
    std::string val; while(pos<json.size() && json[pos] != '"'){ if(json[pos]=='\\' && pos+1<json.size()) pos++; val.push_back(json[pos]); pos++; }
    if(pos>=json.size()) return false; out = val; return true;
}
inline bool extractNumber(const std::string &json, const std::string &key, uint32_t &out) {
    std::string pat = "\"" + key + "\":"; auto pos = json.find(pat); if(pos==std::string::npos) return false; pos += pat.size();
    size_t start = pos; while(pos<json.size() && (isdigit(json[pos]) )) pos++; if(start==pos) return false; out = (uint32_t)std::stoul(json.substr(start, pos-start)); return true;
}
inline bool extractFloat(const std::string &json, const std::string &key, float &out) {
    std::string pat = "\"" + key + "\":"; auto pos = json.find(pat); if(pos==std::string::npos) return false; pos += pat.size();
    size_t start = pos; bool dot=false; while(pos<json.size() && (isdigit(json[pos]) || (!dot && json[pos]=='.'))){ if(json[pos]=='.') dot=true; pos++; }
    if(start==pos) return false; out = std::stof(json.substr(start, pos-start)); return true;
}
inline bool extractBool(const std::string &json, const std::string &key, bool &out) {
    std::string pat = "\"" + key + "\":"; auto pos = json.find(pat); if(pos==std::string::npos) return false; pos += pat.size();
    if (json.compare(pos,4,"true") == 0) { out = true; return true; }
    if (json.compare(pos,5,"false") == 0) { out = false; return true; }
    return false;
}

inline bool deserializeAutoRule(const std::string &json, PlainAutoRule &r) {
    extractBool(json, "enabled", r.enabled);
    extractString(json, "type", r.type);
    extractString(json, "condition", r.condition);
    extractFloat(json, "value1", r.value1);
    extractFloat(json, "value2", r.value2);
    extractString(json, "schedule", r.schedule);
    extractNumber(json, "duration", r.duration);
    extractNumber(json, "lastActivation", r.lastActivation);
    extractBool(json, "isActive", r.isActive);
    return !r.type.empty();
}

#endif // RELAY_SERIALIZATION_H
