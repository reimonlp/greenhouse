# 📡 OTA (Over-The-Air) Updates - Guía Completa

## ¿Qué es OTA?

**OTA** (Over-The-Air) permite actualizar el firmware del ESP32 **de forma inalámbrica** a través de WiFi, sin necesidad de conectar el cable USB.

## Ventajas vs USB

| Aspecto | USB | OTA |
|---------|-----|-----|
| **Acceso físico** | Necesario | No necesario |
| **Velocidad** | Rápido (460800 baud) | Medio (depende WiFi) |
| **Riesgo** | Bajo | Medio (si falla, brick) |
| **Producción** | Impracticable | Ideal |
| **Debugging** | Fácil (Serial) | Difícil |

## 🔐 Configuración de Seguridad

### Password OTA

El sistema está protegido con password para evitar accesos no autorizados:

```cpp
// En include/secrets.h
#define OTA_PASSWORD "greenhouse_ota_2024_secure"
```

**⚠️ IMPORTANTE**: Cambia esta contraseña en producción.

### Hostname

```cpp
#define OTA_HOSTNAME "ESP32_GREENHOUSE_01"
```

El ESP32 se anuncia en la red como `ESP32_GREENHOUSE_01.local`

### Puerto

```cpp
#define OTA_PORT 3232
```

Puerto estándar de Arduino OTA.

## 🚀 Métodos de Upload OTA

### Método 1: Script Automatizado (Recomendado)

```bash
./ota-upload.sh
```

El script hace:
1. ✅ Compila el firmware
2. 🔍 Busca el ESP32 en la red
3. 🏓 Verifica conectividad
4. 📡 Sube el firmware
5. ✅ Confirma éxito

**Output esperado:**
```
╔══════════════════════════════════════════════╗
║  ESP32 Greenhouse - OTA Upload               ║
║  Over-The-Air Wireless Firmware Update       ║
╚══════════════════════════════════════════════╝

📦 Step 1: Building firmware...
✅ Build successful

📊 Firmware size: 947 KB

🔍 Step 2: Discovering ESP32...
   ✅ Found ESP32 at: 192.168.1.45

🏓 Step 3: Checking connectivity...
   ✅ ESP32 is reachable at 192.168.1.45

📡 Step 4: Starting OTA upload...
   Target: 192.168.1.45:3232
   This will take about 30 seconds...

Uploading: [====================] 100%
✅ OTA Upload Successful!
```

### Método 2: PlatformIO Directo

```bash
# Usando IP directa
pio run --target upload --upload-port 192.168.1.45

# Usando hostname
pio run --target upload --upload-port ESP32_GREENHOUSE_01.local
```

### Método 3: espota.py Manual

```bash
python3 ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py \
    -i 192.168.1.45 \
    -p 3232 \
    -f .pio/build/greenhouse-vps-client/firmware.bin \
    -a greenhouse_ota_2024_secure
```

## 🔍 Descubrir IP del ESP32

### Opción 1: Monitor Serial

```bash
pio device monitor
```

Busca líneas como:
```
WiFi connected
IP: 192.168.1.45
```

### Opción 2: Ping mDNS

```bash
ping ESP32_GREENHOUSE_01.local
```

### Opción 3: Router

Accede a tu router y busca dispositivos con hostname `ESP32_GREENHOUSE_01`

### Opción 4: Network Scan

```bash
# Linux/Mac
arp -a | grep -i "esp\|espressif"

# O usar nmap
nmap -sP 192.168.1.0/24
```

## 📊 Proceso de OTA

### 1. Inicio

```
🔄 OTA Update Started: sketch
```

El watchdog se **desactiva automáticamente** para evitar reset durante update.

### 2. Progreso

```
OTA Progress: 10%
OTA Progress: 20%
OTA Progress: 30%
...
OTA Progress: 100%
```

### 3. Finalización

```
✅ OTA Update Completed
```

El ESP32 se **reinicia automáticamente** con el nuevo firmware.

## ⚠️ Problemas Comunes y Soluciones

### Error: "Auth Failed"

**Causa**: Password incorrecto

**Solución**:
```bash
# Verifica el password en secrets.h
grep OTA_PASSWORD include/secrets.h

# Debe coincidir con el usado en upload
```

### Error: "Connect Failed"

**Causa**: ESP32 no alcanzable

**Solución**:
```bash
# Verifica que el ESP32 está en la red
ping ESP32_GREENHOUSE_01.local

# Verifica WiFi conectado
pio device monitor | grep "WiFi connected"
```

### Error: "Begin Failed"

**Causa**: Partición OTA incorrecta o espacio insuficiente

**Solución**:
```bash
# Verifica partitions.csv
cat partitions.csv

# Debe tener partición OTA:
# ota_0, app, ota_0, 0x210000, 0x1E0000
```

### Error: Firewall

**Causa**: Puerto 3232 bloqueado

**Solución** (Linux):
```bash
sudo ufw allow 3232/tcp
```

### Error: ESP32 no responde después de OTA

**Causa**: Firmware corrupto o incompatible

**Solución**:
```bash
# Volver a USB upload
pio run --target upload

# O hacer erase y upload
esptool.py --chip esp32 erase_flash
pio run --target upload
```

## 🛡️ Seguridad en Producción

### 1. Cambiar Password

```cpp
// En secrets.h
#define OTA_PASSWORD "mi_password_super_seguro_2024"
```

### 2. Deshabilitar OTA después del deployment

```cpp
// En vps_ota.h
#define OTA_ENABLED false
```

### 3. Red Segregada

Mantén los ESP32 en una VLAN separada.

### 4. VPN

Para actualizar ESP32 remotos, usa VPN en lugar de exponer puerto 3232.

## 📝 Particiones OTA

El archivo `partitions.csv` define el espacio:

```csv
# Name,     Type, SubType, Offset,  Size,     Flags
nvs,        data, nvs,     0x9000,  0x5000,
otadata,    data, ota,     0xe000,  0x2000,
app0,       app,  ota_0,   0x10000, 0x140000,
app1,       app,  ota_1,   0x150000,0x140000,
spiffs,     data, spiffs,  0x290000,0x170000,
```

- **app0**: Firmware actual
- **app1**: Backup/update slot
- **otadata**: Información de cuál slot es válido

## 🔄 Rollback Automático

Si el nuevo firmware no arranca correctamente (watchdog, crash, etc), el ESP32 **automáticamente vuelve al firmware anterior**.

## 📦 Tamaño Máximo

Con la partición actual:
- **Máximo firmware**: 1,310,720 bytes (~1.25 MB)
- **Actual**: 970,497 bytes (74%)
- **Disponible**: 340,223 bytes (26%)

## 🎯 Casos de Uso

### Desarrollo Local

```bash
# Primera vez: USB
pio run --target upload

# Siguientes veces: OTA
./ota-upload.sh
```

### Producción Remota

```bash
# Via VPN o acceso remoto
ssh usuario@gateway
./ota-upload.sh
```

### Múltiples Dispositivos

```bash
# Script para actualizar varios ESP32
for ip in 192.168.1.45 192.168.1.46 192.168.1.47; do
    pio run --target upload --upload-port $ip
done
```

## 📚 Referencias

- [ESP32 OTA Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html)
- [Arduino OTA](https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA)
- [PlatformIO OTA](https://docs.platformio.org/en/latest/platforms/espressif32.html#over-the-air-ota-update)
