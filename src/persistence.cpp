#include "persistence.h"
#ifdef ARDUINO
#include <Preferences.h>
class PreferencesPersistence : public IPersistence {
    Preferences prefs; 
public:
    bool begin(const char* ns, bool readOnly) override { return prefs.begin(ns, readOnly); }
    bool getBool(const char* key, bool def) override { return prefs.getBool(key, def); }
    void setBool(const char* key, bool value) override { prefs.putBool(key, value); }
    void end() override { prefs.end(); }
};
static PreferencesPersistence gPersistence;
#else
#include <unordered_map>
#include <string>
class InMemoryPersistence : public IPersistence {
    std::string currentNS;
    std::unordered_map<std::string, bool> store; // key => bool
public:
    bool begin(const char* ns, bool readOnly) override { (void)readOnly; currentNS = ns; return true; }
    bool getBool(const char* key, bool def) override {
        auto it = store.find(currentNS + ":" + key);
        if (it == store.end()) return def;
        return it->second;
    }
    void setBool(const char* key, bool value) override { store[currentNS + ":" + key] = value; }
    void end() override {}
    // helper for tests
    void inject(const std::string& ns, const std::string& key, bool value) { store[ns + ":" + key] = value; }
};
static InMemoryPersistence gPersistence;
// Expose injection in tests via reinterpret_cast if needed (not part of public header)
#endif

IPersistence& persistence() { return gPersistence; }
