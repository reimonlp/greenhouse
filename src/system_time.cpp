#include "system.h"
#include "database.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

bool SystemManager::initNTP() {
#if MIN_LOG_LEVEL <= LOG_INFO
    DEBUG_PRINTLN(F("NTP init"));
#endif
    if (!isWiFiConnected()) {
#if MIN_LOG_LEVEL <= LOG_WARNING
    DEBUG_PRINTLN(F("WiFi not connected - cannot initialize NTP"));
#endif
        return false;
    }
    ntpUDP = new WiFiUDP();
    ntpClient = new NTPClient(*ntpUDP, NTP_SERVER, GMT_OFFSET_SEC, 60000);
    ntpClient->begin();
    for (int i = 0; i < 5; i++) {
        if (ntpClient->update()) {
#if MIN_LOG_LEVEL <= LOG_INFO
        DEBUG_PRINTLN(F("NTP sync OK"));
#endif
            bootEpoch = ntpClient->getEpochTime() - (millis() - bootMillis) / 1000;
#if MIN_LOG_LEVEL <= LOG_DEBUG
            DEBUG_PRINTLN(String(F("Time: ")) + getCurrentTimeString());
#endif
            return true;
        }
        delay(2000);
    }
    ntpFailureCount++;
#if MIN_LOG_LEVEL <= LOG_WARNING
    DEBUG_PRINTLN(F("NTP sync fail"));
#endif
    return false;
}

unsigned long SystemManager::getCurrentTimestamp() {
    if (ntpClient && ntpClient->isTimeSet()) {
        return ntpClient->getEpochTime();
    }
    if (bootEpoch != 0) {
        unsigned long elapsedSec = (millis() - bootMillis) / 1000;
        return bootEpoch + elapsedSec;
    }
    return (millis() / 1000);
}

String SystemManager::getCurrentTimeString() {
    if (ntpClient && ntpClient->isTimeSet()) {
        return ntpClient->getFormattedTime();
    }
    return "Time not synced";
}

bool SystemManager::isTimeSync() {
    return (ntpClient && ntpClient->isTimeSet()) || (bootEpoch != 0);
}
