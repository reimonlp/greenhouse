#ifndef RELAYS_H
#define RELAYS_H

#include "config.h"
#include "sensors.h"
#include <ArduinoJson.h>

struct AutoRule { // Mantiene String para firmware; se adapta a RuleDefinition al evaluar
    bool enabled;
    String type;
    String condition;
    float value1, value2;
    String schedule;      // "HH:MM-HH:MM"
    unsigned long duration;
    unsigned long lastActivation;
    bool isActive;
    // --- Métricas (O2) ---
    uint32_t evalCount = 0;            // Total evaluaciones desde arranque
    uint32_t activationCount = 0;      // Total activaciones (transiciones OFF->ON automáticas)
    uint32_t hourlyEvalCount = 0;      // Evaluaciones en la ventana horaria actual
    uint32_t hourlyActivationCount = 0;// Activaciones en la ventana horaria actual
    uint8_t hourlyWindowHour = 0;      // Hora local (0-23) de la ventana actual
    // --- Métricas ampliadas (O3) ---
    unsigned long lastEvalAt = 0;      // millis() del último ciclo de evaluación
    bool lastDecision = false;         // Resultado de la última evaluación (true=quería ON)
    // Ventana deslizante 60m (6 buckets de 10m) para suavizar reinicios de hora
    uint16_t evalBuckets[6];
    uint16_t actBuckets[6];
    uint8_t currentBucket = 0;         // Índice de bucket activo
    uint32_t bucketBaseMinutes = 0;    // Minuto absoluto (millis/60000) en el que inició el bucket actual
    AutoRule():enabled(false),value1(0),value2(0),duration(0),lastActivation(0),isActive(false){}
};

class RelayManager {
private:
    RelayState relayStates[4];
    AutoRule autoRules[4];
    const int relayPins[4] = {RELAY_LUCES_PIN, RELAY_VENTILADOR_PIN, RELAY_BOMBA_PIN, RELAY_CALEFACTOR_PIN};
    // Nombres de relays en PROGMEM para ahorrar RAM (convertidos on-demand)
    static const char relayName0[] PROGMEM; // luces
    static const char relayName1[] PROGMEM; // ventilador
    static const char relayName2[] PROGMEM; // bomba
    static const char relayName3[] PROGMEM; // calefactor
    static const char* const relayNames_P[4] PROGMEM;
    String relayName(uint8_t idx) const;
    
    unsigned long lastAutoCheck;
    bool safetyLimitsEnabled;
    
public:
    RelayManager();
    
    bool begin();
    void update(); // Llamar en loop principal
    
    // Control manual
    bool setRelay(int relayIndex, bool state);
    bool toggleRelay(int relayIndex);
    bool getRelayState(int relayIndex);
    
    // Control de modo
    bool setRelayMode(int relayIndex, RelayMode mode);
    RelayMode getRelayMode(int relayIndex);
    
    // Reglas automáticas
    bool setAutoRule(int relayIndex, const String& ruleJson);
    String getAutoRule(int relayIndex);
    bool enableAutoRule(int relayIndex, bool enable);
    
    // Límites de seguridad
    void enableSafetyLimits(bool enable);
    bool checkSafetyLimits();
    
    // Estado del sistema
    String getSystemStatus();
    SystemStats getRelayStatistics();
    void resetStatistics();
    
    // Pausa del sistema
    void pauseSystem(bool pause);
    bool isSystemPaused();
    
    // Recuperación tras corte de luz
    void restoreStateAfterPowerLoss();
    void saveStateToNVS();
    void loadStateFromNVS();
    
private:
    void processAutoRules();
    void enforceTimeouts();
    
    String lastError;
    bool systemPaused;
    unsigned long pauseStartTime;
};

extern RelayManager relays;

#endif // RELAYS_H