// pins.h - central pin mapping for ESP32-C3 Super Mini
// ESP32-C3 has 22 GPIO pins total, with flash and USB JTAG using some
#pragma once

// ESP32-C3 Super Mini Pin Mapping (based on actual pinout):
// Left side:   A0(GPIO0), A1(GPIO1), A2(GPIO2), A3(GPIO3), A4(GPIO4), SCK(GPIO10)
// Right side:  GPIO5, GPIO6, GPIO7, GPIO8(SDA), GPIO9(SCL), GPIO20(RX), GPIO21(TX)
// SPI Pins:    GPIO5(MISO), GPIO6(MOSI), GPIO7(SS) - DON'T USE for other functions

// Reserved/Used:
// - GPIO 0-1: Boot/ADC - CAN be used but careful with ADC if boot is needed
// - GPIO 2-7: Some are ADC, some are SPI Flash (2,3 are ADC, 5,6,7 are SPI)
// - GPIO 8-9: I2C (SDA/SCL) - available but marked on board
// - GPIO 10: SCK pin - can be used (labeled on board)
// - GPIO 20-21: UART (RX/TX) - labeled but can be reassigned

// RELAY PINS - Using GPIO 2, 3, 4, 10 (ADC pins that are safe to use)
#define RELAY_LUCES_PIN       2   // GPIO2 (A2) → Lights
#define RELAY_VENTILADOR_PIN  3   // GPIO3 (A3) → Fan
#define RELAY_BOMBA_PIN       4   // GPIO4 (A4) → Pump
#define RELAY_CALEFACTOR_PIN  10  // GPIO10 (SCK) → Heater

// SENSOR PINS
#define DHT_PIN               1   // GPIO1 (A1) → DHT11 sensor (ADC pin, safe to use)
// Soil moisture on ADC - Use GPIO0 (A0) for analog input
#define SOIL_MOISTURE_1_PIN   0   // GPIO0 (A0) → Soil Moisture Analog (ADC1_CH0)

// Control / indicators (GPIO with I2C labels but can be repurposed)
#define STATUS_LED_PIN        8   // GPIO8 (SDA) → LED (won't conflict if I2C not used)
#define RESCUE_FORMAT_PIN     9   // GPIO9 (SCL) → Rescue pin (pull-up + jumper to GND)

// NOTE: GPIO 5,6,7 are SPI pins - DON'T use them for relay/sensor signals
// NOTE: GPIO 20,21 are UART - available but marked on board
