/*
 * ESP32 Greenhouse Control System
 * Sistema de control de invernadero para cultivo de marihuana medicinal
 * 
 * Funcionalidades:
 * - Control automático/manual de relays (luces, ventilador, bomba, calefactor)
 * - Monitoreo de sensores (DHT11, temperatura, humedad de suelo)
 * - API REST con autenticación
 * - Logging a MongoDB en la nube
 * - Dashboard web embebido
 * - OTA updates y WiFi manager
 * - Detección de cortes de luz
 * - Estadísticas y análisis
 * 
 * Hardware necesario:
 * - ESP32-C3
 * - Módulo de 4 relays
 * - DHT11 (temp/humedad ambiental)
 * - Sensores de temperatura adicionales (DS18B20)
 * - Sensores capacitivos de humedad de suelo
 * - LED de estado, tecla de pausa (buzzer eliminado)
 * - Display OLED (deshabilitado)
 * 
 * Author: Sistema para reimon
 * Version: 1.0.0
 */

#include <Arduino.h>
#include <LittleFS.h>
#include <nvs_flash.h>
#include <Preferences.h>
#include <esp_system.h> // Para esp_reset_reason()

#include "config.h"
#include "system.h"
#include "sensors.h"
#include "relays.h"
#include "api.h"
#include "database.h"
#include "dashboard.h"
#include "pins.h"
#include "embedded_assets.h"

// Instancias globales (definidas en sus respectivos .cpp)
extern SystemManager system_manager;
extern SensorManager sensors;
extern RelayManager relays;
extern APIManager api;
extern DatabaseManager database;
extern DashboardManager dashboard;

// Variables globales
unsigned long lastSensorRead = 0;
unsigned long lastLogSend = 0;
unsigned long lastHeartbeat = 0;
bool systemInitialized = false;

void setup() {
    DEBUG_SERIAL_BEGIN(115200);
    delay(500);

    pinMode(RESCUE_FORMAT_PIN, INPUT_PULLUP);
    bool rescue = digitalRead(RESCUE_FORMAT_PIN) == LOW;
    if (rescue) {
        DEBUG_PRINTLN(F("[RESCUE] Rescue pin LOW -> formatting filesystem"));
        if (LittleFS.begin(true)) { LittleFS.format(); LittleFS.begin(true); }
#ifdef ENABLE_EMBEDDED_ASSET_RESTORE
        restoreEmbeddedAssets(true);
#endif
    }

    // === Crash recovery logic (reemplaza heurística basada en millis que producía falsos positivos) ===
    // Problema anterior: comparaba millis() tras reinicio con valor previo; tras cualquier boot millis <60s → contaba como reboot rápido y tras 3 reinicios formateaba.
    // Nueva lógica: sólo incrementa contador si la razón de reinicio indica crash real (WDT, panic, brownout).
    Preferences prefs; prefs.begin("boot", false);
    uint8_t failCount = prefs.getUChar("fc", 0);
    uint8_t formatCount = prefs.getUChar("fmt_cnt", 0);
    unsigned long lastFormatTime = prefs.getULong("fmt_time", 0);
    unsigned long currentTime = millis() / 1000; // segundos desde boot
    
    esp_reset_reason_t rr = esp_reset_reason();
    bool crashReset = (rr == ESP_RST_WDT || rr == ESP_RST_TASK_WDT || rr == ESP_RST_INT_WDT || rr == ESP_RST_PANIC || rr == ESP_RST_BROWNOUT);

    if (crashReset) {
        failCount++;
    } else if (failCount > 0) {
        // Reinicio normal limpia contador acumulado por falsos positivos previos
        failCount = 0;
    }
    prefs.putUChar("fc", failCount);

    DEBUG_PRINTF("[BOOT] Reset reason=%d crashReset=%s failCount=%u formatCount=%u\n", 
                 (int)rr, crashReset?"true":"false", failCount, formatCount);

    const uint8_t CRASH_FORMAT_THRESHOLD = 3; // mismo umbral que antes pero ahora significativo
    const uint8_t MAX_FORMATS_PER_DAY = 2;    // límite de formateos en 24h
    bool safeModeRequired = false;
    
    if (failCount >= CRASH_FORMAT_THRESHOLD) {
        // Verificar si excedimos límite de formateos
        if (formatCount >= MAX_FORMATS_PER_DAY && (currentTime < 86400 || lastFormatTime > 0)) {
            DEBUG_PRINTLN(F("[RECOVERY] Too many formats in 24h - entering SAFE MODE"));
            safeModeRequired = true;
            prefs.putBool("safe_mode", true);
            prefs.putUChar("fc", 0); // resetear contador de fallos
        } else {
            DEBUG_PRINTLN(F("[RECOVERY] Repeated crash resets detected. Formatting LittleFS..."));
            if (LittleFS.begin(true)) {
                LittleFS.format();
                LittleFS.begin(true);
                formatCount++;
                prefs.putUChar("fc", 0);
                prefs.putUChar("fmt_cnt", formatCount);
                prefs.putULong("fmt_time", currentTime);
                DEBUG_PRINTF("[RECOVERY] Filesystem formatted (%u/2 formats today). Restoring assets.\n", formatCount);
#ifdef ENABLE_EMBEDDED_ASSET_RESTORE
                restoreEmbeddedAssets(true);
#endif
            } else {
                DEBUG_PRINTLN(F("[RECOVERY] Failed to mount LittleFS for format."));
            }
        }
    }
    
    // Resetear contador de formateos cada 24h
    if (lastFormatTime > 0 && currentTime > 86400) {
        prefs.putUChar("fmt_cnt", 0);
        prefs.putULong("fmt_time", 0);
    }
    
    bool inSafeMode = safeModeRequired || prefs.getBool("safe_mode", false);
    prefs.end();
    
    // En safe mode: configuración reducida
    if (inSafeMode) {
        DEBUG_PRINTLN(F("[SAFE MODE] Running with minimal features"));
        // TODO: implementar flags para deshabilitar reglas auto, WiFi agresivo, etc.
    }

    DEBUG_PRINTLN(F("=== ESP32 Greenhouse Control System ==="));
    DEBUG_PRINTLN(F("Version: " FIRMWARE_VERSION));
    DEBUG_PRINTLN(F("Build: " BUILD_DATE));
    DEBUG_PRINTLN(F("=========================================="));

    // Inicializar sistema de archivos
    if (!LittleFS.begin(true)) {
    DEBUG_PRINTLN(F("ERROR: Failed to mount LittleFS"));
        ESP.restart();
    }
    DEBUG_PRINTLN(F("[OK] LittleFS mounted"));
#if 1 // Diagnóstico: listar archivos en LittleFS al arranque
    {
        DEBUG_PRINTLN(F("[FS] Listing root files:"));
        File root = LittleFS.open("/");
        if (!root) {
            DEBUG_PRINTLN(F("[FS] Failed to open root directory"));
        } else {
            File file = root.openNextFile();
            bool foundScript = false;
            while (file) {
                DEBUG_PRINTF("[FS] %s (%u bytes)\n", file.name(), (unsigned)file.size());
                if (String(file.name()) == "/script.js") foundScript = true;
                file = root.openNextFile();
            }
            DEBUG_PRINTF("[FS] script.js present: %s\n", foundScript?"YES":"NO");
            if (!foundScript) {
                DEBUG_PRINTLN(F("[FS][WARN] /script.js missing -> dashboard JS 404"));
            }
        }
    }
#endif
#ifdef ENABLE_EMBEDDED_ASSET_RESTORE
    // Ensure critical assets exist (only overwrite if missing or size mismatch)
    restoreEmbeddedAssets(false);
#endif
    
    // Inicializar gestor del sistema (WiFi, NTP, OTA, Watchdog)
    if (!system_manager.begin()) {
    DEBUG_PRINTLN(F("ERROR: System manager initialization failed"));
        ESP.restart();
    }
    DEBUG_PRINTLN(F("[OK] System manager initialized"));
    
    // Inicializar base de datos y logging
    if (!database.begin()) {
    DEBUG_PRINTLN(F("WARNING: Database initialization failed - running in offline mode"));
    } else {
    DEBUG_PRINTLN(F("[OK] Database manager initialized"));
    }
    
    // Log del inicio del sistema
    database.logSystemEvent("system_boot", 
        "ESP32-C3 Greenhouse Control System started - Version: " FIRMWARE_VERSION);
    
    // Inicializar sensores
    if (!sensors.begin()) {
    DEBUG_PRINTLN(F("ERROR: Sensor initialization failed"));
        database.logError("sensors", "Failed to initialize sensors");
        system_manager.setState(SYSTEM_ERROR);
    } else {
    DEBUG_PRINTLN(F("[OK] Sensors initialized"));
        database.logSystemEvent("sensors_init", "All sensors initialized successfully");
    }
    
    // Inicializar control de relays
    if (!relays.begin()) {
    DEBUG_PRINTLN(F("ERROR: Relay initialization failed"));
        database.logError("relays", "Failed to initialize relays");
        system_manager.setState(SYSTEM_ERROR);
    } else {
    DEBUG_PRINTLN(F("[OK] Relays initialized"));
        database.logSystemEvent("relays_init", "All relays initialized successfully");
        
        // Cargar estado anterior si hubo corte de luz
        relays.loadStateFromNVS();
        if (system_manager.wasPowerLost()) {
            DEBUG_PRINTLN(F("Power loss detected - restoring previous state"));
            database.logSystemEvent("power_restored", "System recovered from power loss");
            relays.restoreStateAfterPowerLoss();
            system_manager.handlePowerRestoration();
        }
    }
    
    // Inicializar API REST
    if (!api.begin()) {
    DEBUG_PRINTLN(F("ERROR: API initialization failed"));
        database.logError("api", "Failed to initialize API server");
    } else {
    DEBUG_PRINTLN(String(F("[OK] API server started on port ")) + String(API_PORT));
        database.logSystemEvent("api_init", "API REST server started successfully");
    }
    
    // Inicializar dashboard web
    if (!dashboard.begin()) {
    DEBUG_PRINTLN(F("WARNING: Dashboard initialization failed"));
        database.logError("dashboard", "Failed to initialize web dashboard");
    } else {
    DEBUG_PRINTLN(F("[OK] Web dashboard initialized"));
        database.logSystemEvent("dashboard_init", "Web dashboard initialized successfully");
    }
    
    // Sistema inicializado correctamente
    system_manager.setState(SYSTEM_NORMAL);
    systemInitialized = true;
    
    DEBUG_PRINTLN(F("=========================================="));
    DEBUG_PRINTLN(F("System initialization completed!"));
    DEBUG_PRINTLN(String(F("Web Dashboard: http://")) + WiFi.localIP().toString());
    DEBUG_PRINTLN(String(F("API Endpoint: http://")) + WiFi.localIP().toString() + F("/api"));
    DEBUG_PRINTLN(F("=========================================="));
    
    // Buzzer eliminado: no beep de confirmación
    
    database.logSystemEvent("system_ready", 
        "System fully initialized - IP: " + WiFi.localIP().toString());
}

void loop() {
    // Alimentar watchdog
    system_manager.feedWatchdog();
    
    // Actualizar gestor del sistema
    system_manager.update();
    
    // Manejar OTA updates
    system_manager.handleOTA();
    
    // Actualizar control de relays (automático)
    relays.update();
    
    // Leer sensores periódicamente
    if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL_MS) {
        if (sensors.readSensors()) {
            SensorData data = sensors.getCurrentData();
            
            // Log de datos de sensores a la base de datos
            database.logSensorData(data);
            // Emitir actualización en vivo vía SSE
            dashboard.sendSensorUpdate(data);
            
            // Verificar límites de seguridad (solo si hay datos válidos de sensores)
            SensorData sensorData = sensors.getCurrentData();
            if (sensorData.valid && !relays.checkSafetyLimits()) {
                DEBUG_PRINTLN(F("WARNING: Safety limits exceeded!"));
                database.logError("safety", "Safety limits exceeded");
                // Buzzer eliminado (antes alerta sonora)
            }
        } else {
            // Only show detailed sensor errors in verbose mode when sensors are expected
#if VERBOSE_LOGS
            DEBUG_PRINTLN(F("INFO: Sensors not available"));
            database.logInfo("sensors", "No sensor data: " + sensors.getLastError());
#endif
        }
        lastSensorRead = millis();
    }
    // Si una conversión DS18 está en progreso, intentar completar sin esperar al próximo intervalo
    if (sensors.isDs18InProgress()) {
        sensors.readSensors(); // reintenta para cerrar ciclo una vez conversion lista
    }
    // Avanzar muestreo no bloqueante de suelo entre lecturas principales
    sensors.updateSoilSampling();
    
    // Lógica periódica de flush de logs con jitter / batch
    database.periodic();
    
    // Heartbeat del sistema
    if (millis() - lastHeartbeat >= 60000) { // Cada minuto
        system_manager.sendHeartbeat(); // Ya registra estado
        relays.saveStateToNVS();        // Persistir estado crítico
        lastHeartbeat = millis();
        // Enviar heartbeat SSE (heap / uptime)
        dashboard.sendSystemHeartbeat();
    }
    
    // Manejar clientes de la API
    api.handleClient();
    
    // Verificar si el sistema debe estar pausado
    if (system_manager.isPauseButtonPressed()) {
        bool currentlyPaused = relays.isSystemPaused();
        relays.pauseSystem(!currentlyPaused);
        
        if (!currentlyPaused) {
            DEBUG_PRINTLN(F("System PAUSED by physical button"));
            database.logSystemEvent("system_paused", "Paused by physical button");
            system_manager.setState(SYSTEM_PAUSED);
            // Buzzer eliminado (antes beep pausa)
        } else {
            DEBUG_PRINTLN(F("System RESUMED by physical button"));
            database.logSystemEvent("system_resumed", "Resumed by physical button");
            system_manager.setState(SYSTEM_NORMAL);
            // Buzzer eliminado (antes beep reanudar)
        }
    }
    
    // Actualizar LED de estado
    system_manager.updateStatusLed();
    
    // Pequeño delay para no saturar el CPU
    delay(100);
}

/*
 * Función llamada cuando se detecta un reinicio por watchdog
 */
void esp_task_wdt_isr_user_handler(void) {
    database.logError("watchdog", "System restarted by watchdog timer");
    system_manager.handlePowerRestoration();
}