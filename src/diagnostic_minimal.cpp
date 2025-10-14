// Minimal diagnostic firmware compiled only when BUILD_MINIMAL is defined
#ifdef BUILD_MINIMAL
#include <Arduino.h>

const int LED_PIN = 8;

void setup() {
  DEBUG_SERIAL_BEGIN(115200);
  delay(200);
  DEBUG_PRINTLN(F("[DIAG] Minimal firmware booting..."));
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  static unsigned long last = 0;
  unsigned long now = millis();
  if (now - last >= 1000) {
    last = now;
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  DEBUG_PRINTLN(F("[DIAG] alive"));
  }
}
#endif
