#ifndef NVS_UTILS_H
#define NVS_UTILS_H
#include <Preferences.h>
#include <Arduino.h>

// Intenta abrir un namespace NVS en modo solo lectura y si falla reintenta RW.
// Devuelve true si queda abierto, false si no fue posible.
inline bool safePrefsBegin(Preferences &prefs, const char *ns, bool readOnlyDesired) {
    // Primer intento: si el usuario pide readOnly, probar readOnly
    if (readOnlyDesired) {
        if (prefs.begin(ns, true)) return true;
        // Si no existe el namespace, caerá aquí y reintentamos RW para crearlo.
    }
    // Reintento RW para crear si no existe
    if (prefs.begin(ns, false)) {
        return true;
    }
    return false;
}

#endif // NVS_UTILS_H
