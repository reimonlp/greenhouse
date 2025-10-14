// Simple time source abstraction for testability (T4)
#ifndef TIME_SOURCE_H
#define TIME_SOURCE_H

#include <stdint.h>

class ITimeSource {
public:
    virtual unsigned long millis() const = 0;
    virtual ~ITimeSource() {}
};

// Firmware implementation will just wrap ::millis()
class ArduinoTimeSource : public ITimeSource {
public:
    unsigned long millis() const override; // defined in .cpp
};

// Access global time source (defaults to ArduinoTimeSource). Tests can swap.
ITimeSource& timeSource();
void setTimeSource(ITimeSource* src); // pass nullptr to reset to default

#endif // TIME_SOURCE_H
