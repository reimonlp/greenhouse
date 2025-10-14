#include "system.h"
#include "database.h"

// Módulo separado para eventos del sistema (heartbeat, restauración de energía, etc.)
// Centraliza lógica de generación para reducir tamaño de system.cpp

namespace SystemEvents {
    void logPowerRestored(unsigned long outageSeconds) {
        database.logPowerEvent(false, system_manager.getCurrentTimestamp());
        database.logSystemEvent("power_restored", String("Power restored after ") + outageSeconds + " seconds");
    }
    void logHeartbeat() {
        database.logSystemEvent("heartbeat", system_manager.getSystemInfo());
    }
}

// Global wrapper functions (C++ linkage) used by system.cpp
void SystemEvents_logPowerRestored(unsigned long outageSeconds) { SystemEvents::logPowerRestored(outageSeconds); }
void SystemEvents_logHeartbeat() { SystemEvents::logHeartbeat(); }
