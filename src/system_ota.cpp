#include "system.h"
#include "database.h"
#include <ArduinoOTA.h>

bool SystemManager::initOTA() {
#ifdef FEATURE_DISABLE_OTA
    return false;
#endif
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTLN(F("OTA init"));
#endif
    ArduinoOTA.setPort(OTA_PORT);
    ArduinoOTA.setHostname("esp32-greenhouse");
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTLN(String(F("OTA updating ")) + type);
#endif
        database.logSystemEvent("ota_start", "OTA update started: " + type);
    });

    ArduinoOTA.onEnd([]() {
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTLN(F("\nOTA Update completed"));
#endif
        database.logSystemEvent("ota_complete", "OTA update completed successfully");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static unsigned long lastProgress = 0;
        unsigned long currentProgress = (progress / (total / 100));
        if (currentProgress != lastProgress) {
#if MIN_LOG_LEVEL <= LOG_DEBUG
            DEBUG_PRINTF("Progress: %u%%\r", currentProgress);
#endif
            lastProgress = currentProgress;
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
#if MIN_LOG_LEVEL <= LOG_ERROR
        DEBUG_PRINTF("Error[%u]: ", error);
#endif
        String errorMsg = "OTA Error: ";
        if (error == OTA_AUTH_ERROR) errorMsg += "Auth Failed";
        else if (error == OTA_BEGIN_ERROR) errorMsg += "Begin Failed";
        else if (error == OTA_CONNECT_ERROR) errorMsg += "Connect Failed";
        else if (error == OTA_RECEIVE_ERROR) errorMsg += "Receive Failed";
        else if (error == OTA_END_ERROR) errorMsg += "End Failed";
#if MIN_LOG_LEVEL <= LOG_ERROR
    DEBUG_PRINTLN(errorMsg);
#endif
        database.logError("ota", errorMsg);
    });

    ArduinoOTA.begin();
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTLN(F("OTA ready"));
#endif
    return true;
}
