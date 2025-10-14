#include "config_schema.h"

bool migrateConfigSchema(JsonDocument& doc, uint32_t& fromVersion) {
    uint32_t ver = doc["schema_version"] | 0;
    fromVersion = ver;
    if (ver == 0) {
        // Legacy configs: inject defaults as needed
        // (placeholder: add future mandatory fields here)
    }
    while (ver < CONFIG_SCHEMA_VERSION) {
        switch (ver) {
            case 0:
                // Migration 0 -> 1 (example placeholder)
                // If a new field is required, set a sensible default here.
                break;
            default:
                // Unknown older version path; fail
                return false;
        }
        ver++;
    }
    doc["schema_version"] = CONFIG_SCHEMA_VERSION;
    return true;
}
