#include "system.h"
#include <esp_task_wdt.h>

bool SystemManager::initWatchdog() {
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTLN(F("WDT init"));
#endif
    esp_task_wdt_init(WATCHDOG_TIMEOUT_SEC, true);
    esp_task_wdt_add(NULL);
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTLN(String(F("WDT ")) + String(WATCHDOG_TIMEOUT_SEC) + F("s"));
#endif
    return true;
}

void SystemManager::feedWatchdog() {
    esp_task_wdt_reset();
}
