// embedded_assets.h
// Provides firmware-embedded copies of critical web UI assets so they can be
// restored automatically after a LittleFS format or corruption healing.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "config.h"

#if __has_include(<Arduino.h>)
#include <Arduino.h>
#endif

#ifdef ENABLE_EMBEDDED_ASSET_RESTORE

struct EmbeddedAsset {
    const char *path;          // Destination path in LittleFS (e.g. /index.html)
    const char *content;       // Null-terminated content (stored in flash/PROGMEM)
    size_t size;               // Size without terminating null
};

// Restores missing or size-mismatched assets. If force==true, always overwrites.
// Returns true if all assets are present (after restoration) and valid.
bool restoreEmbeddedAssets(bool force = false, Stream *logOut = &Serial);

#endif // ENABLE_EMBEDDED_ASSET_RESTORE
