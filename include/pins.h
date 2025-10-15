// pins.h - central pin mapping for NodeMCU ESP32
#pragma once

// Relays
#define RELAY_LUCES_PIN       16  // GPIO16
#define RELAY_VENTILADOR_PIN  17  // GPIO17
#define RELAY_BOMBA_PIN       18  // GPIO18
#define RELAY_CALEFACTOR_PIN  19  // GPIO19

// Sensores
#define DHT_PIN               23  // GPIO23 → DHT11 sensor
// External temperature sensor pins removed (NTC/DS18B20 not used)
#define SOIL_MOISTURE_1_PIN   34  // GPIO34 (input only)
#define SOIL_MOISTURE_2_PIN   35  // GPIO35 (input only)

// Control / indicadores
#define PAUSE_BUTTON_PIN      4   // GPIO4
#define STATUS_LED_PIN        2   // GPIO2 (on-board)
#define RESCUE_FORMAT_PIN     25  // GPIO25 (pull-up + jumper a GND para forzar format)
