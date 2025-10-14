// Persistence abstraction to decouple Preferences/NVS for testability
#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <stdbool.h>

class IPersistence {
public:
    virtual bool begin(const char* ns, bool readOnly) = 0;
    virtual bool getBool(const char* key, bool defaultVal) = 0;
    virtual void setBool(const char* key, bool value) = 0;
    virtual void end() = 0;
    virtual ~IPersistence() {}
};

// Access global persistence implementation (firmware or in-memory)
IPersistence& persistence();

#endif // PERSISTENCE_H
