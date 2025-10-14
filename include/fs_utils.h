// fs_utils.h - centralized filesystem helpers
#pragma once
#ifdef ARDUINO
#include <LittleFS.h>

// Mount guard: ensures LittleFS.begin called once. formatOnFail optional (default true only for first attempt in setup usage)
inline bool ensureFS(bool formatOnFail = true) {
    static bool mounted = false;
    if (mounted) return true;
    if (LittleFS.begin(formatOnFail)) { mounted = true; return true; }
    return false;
}

// Ensure a file exists (empty) without overwriting existing content.
inline bool ensureFileExists(const char* path) {
    if (!ensureFS()) return false;
    if (LittleFS.exists(path)) return true;
    File f = LittleFS.open(path, "w"); if (!f) return false; f.close();
    return true;
}
#endif
