#ifndef CONFIG_SCHEMA_H
#define CONFIG_SCHEMA_H

#include <ArduinoJson.h>
#include "config.h"

// Attempt to migrate a JSON document in-place to current CONFIG_SCHEMA_VERSION.
// Returns true if migration succeeded (or not needed).
bool migrateConfigSchema(JsonDocument& doc, uint32_t& fromVersion);

#endif // CONFIG_SCHEMA_H
