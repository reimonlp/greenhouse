# ESP32 Log Levels

El firmware soporta 4 niveles de logging para controlar la verbosidad de los mensajes en el monitor serial.

## Niveles Disponibles

| Nivel | Valor | Descripción | Uso Recomendado |
|-------|-------|-------------|-----------------|
| **NONE** | 0 | Sin output serial | Producción final, máxima eficiencia |
| **ERROR** | 1 | Solo errores críticos | Producción, diagnóstico mínimo |
| **WARN** | 2 | Errores + advertencias | Producción, monitoreo normal |
| **INFO** | 3 | + Eventos importantes | Staging, verificación |
| **DEBUG** | 4 | + Todos los detalles | Desarrollo (default) |

## Configuración

### Método 1: platformio.ini (Recomendado)

Edita `platformio.ini` y agrega en `build_flags`:

```ini
build_flags = 
    ; ...otras flags...
    -D LOG_LEVEL=2  ; ERROR + WARN solamente
```

### Método 2: config.h

Modifica el valor default en `include/config.h`:

```cpp
#ifndef LOG_LEVEL
    #define LOG_LEVEL 2  // Cambiar de 4 a 2
#endif
```

## Macros Disponibles

### Nuevas Macros (Recomendadas)

```cpp
LOG_ERROR("Connection failed");
LOG_ERRORF("Failed after %d attempts", count);

LOG_WARN("Low memory");
LOG_WARNF("Memory: %d bytes free", free);

LOG_INFO("System started");
LOG_INFOF("Uptime: %lu seconds", uptime);

LOG_DEBUG("Sensor reading...");
LOG_DEBUGF("Temp: %.1f°C", temp);
```

### Macros Legacy (Compatibilidad)

Las macros existentes (`DEBUG_PRINT`, `DEBUG_PRINTLN`, `DEBUG_PRINTF`) siguen funcionando y se mapean a nivel DEBUG.

## Ejemplos de Uso

### Desarrollo (LOG_LEVEL=4)

```
ℹ INFO: System started
Sensor reading...
Temp: 25.3°C
✓ WebSocket connected
```

### Producción (LOG_LEVEL=2)

```
⚠ WARN: Retry connection (attempt 3)
```

### Producción Silenciosa (LOG_LEVEL=1)

```
❌ ERROR: Auth failed - invalid token
```

## Impacto en Rendimiento

- **LOG_LEVEL=0**: Sin overhead, macros compiladas a `((void)0)`
- **LOG_LEVEL=1-2**: Mínimo impacto, solo errores/warnings
- **LOG_LEVEL=3**: Impacto bajo, eventos principales
- **LOG_LEVEL=4**: Impacto moderado, todos los mensajes

## Recomendaciones

- **Desarrollo**: Usar LOG_LEVEL=4 para ver todo
- **Testing**: Usar LOG_LEVEL=3 para eventos importantes
- **Staging**: Usar LOG_LEVEL=2 para monitoreo
- **Producción**: Usar LOG_LEVEL=1 o 0 para eficiencia máxima
