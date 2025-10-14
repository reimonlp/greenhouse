#include "time_source.h"
#ifdef ARDUINO
#include <Arduino.h>
#endif

unsigned long ArduinoTimeSource::millis() const {
#ifdef ARDUINO
    return ::millis();
#else
    // Fallback; tests will override anyway.
    static unsigned long fake = 0; return fake += 10;
#endif
}

static ArduinoTimeSource defaultSrc;
static ITimeSource* active = &defaultSrc;

ITimeSource& timeSource() { return *active; }
void setTimeSource(ITimeSource* src) { active = src ? src : &defaultSrc; }
