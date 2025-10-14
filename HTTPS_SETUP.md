# HTTPS Configuration Guide for Greenhouse Control System

**Target IP:** 10.0.0.104  
**Fecha:** 13 de octubre de 2025

---

## 📋 Opciones de Implementación HTTPS

### Opción 1: Certificado Autofirmado en ESP32 (Recomendado para Red Local)

#### Ventajas
- ✅ No requiere servicios externos
- ✅ Funciona completamente offline
- ✅ Configuración simple
- ✅ Sin costos adicionales

#### Desventajas
- ❌ Advertencia de seguridad en navegadores (bypass manual requerido)
- ❌ No válido para acceso externo

#### Implementación

**1. Generar Certificado y Clave Privada**

```bash
# Generar certificado autofirmado válido por 365 días
openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
  -keyout greenhouse_key.pem \
  -out greenhouse_cert.pem \
  -subj "/C=AR/ST=BuenosAires/L=CABA/O=Greenhouse/CN=10.0.0.104"

# Convertir a formato DER para ESP32
openssl x509 -in greenhouse_cert.pem -out greenhouse_cert.der -outform DER
openssl rsa -in greenhouse_key.pem -out greenhouse_key.der -outform DER
```

**2. Incrustar Certificados en el Código**

Añadir en `include/certificates.h`:

```cpp
#ifndef CERTIFICATES_H
#define CERTIFICATES_H

// Certificado en formato DER convertido a array de bytes
const uint8_t server_cert_der[] PROGMEM = {
  // Contenido del greenhouse_cert.der
  0x30, 0x82, 0x03, 0x...
};

const uint8_t server_key_der[] PROGMEM = {
  // Contenido del greenhouse_key.der
  0x30, 0x82, 0x02, 0x...
};

const size_t server_cert_der_len = sizeof(server_cert_der);
const size_t server_key_der_len = sizeof(server_key_der);

#endif // CERTIFICATES_H
```

**3. Modificar API Manager**

En `src/api.cpp`:

```cpp
#include "certificates.h"

bool APIManager::begin() {
    DEBUG_PRINTLN(F("Starting HTTPS API server..."));
    
    // Crear servidor HTTPS
    server = new AsyncWebServerSSL(API_PORT);
    
    // Cargar certificados
    server->setSslCertificate(server_cert_der, server_cert_der_len);
    server->setSslPrivateKey(server_key_der, server_key_der_len);
    
    setupRoutes();
    server->begin();
    
    DEBUG_PRINTLN(F("HTTPS server started on port 443"));
    return true;
}
```

**4. Actualizar platformio.ini**

```ini
[env:greenhouse]
lib_deps =
    ...
    ESP Async WebServer SSL  # Librería con soporte HTTPS
    
build_flags =
    ...
    -D ENABLE_HTTPS=1
    -D API_PORT=443
```

**5. Acceder desde el navegador**

```
https://10.0.0.104/
```

**Nota:** El navegador mostrará una advertencia. Haz clic en "Avanzado" > "Continuar de todos modos".

---

### Opción 2: Reverse Proxy con Nginx (Recomendado para Producción)

#### Ventajas
- ✅ Certificado válido sin advertencias
- ✅ Mejor rendimiento (offloading SSL)
- ✅ Permite múltiples servicios detrás del proxy
- ✅ Fácil renovación de certificados

#### Desventajas
- ❌ Requiere servidor adicional (Raspberry Pi, NAS, PC)
- ❌ Configuración más compleja

#### Implementación

**1. Instalar Nginx en Raspberry Pi/Ubuntu**

```bash
sudo apt update
sudo apt install nginx certbot python3-certbot-nginx
```

**2. Configurar DuckDNS (DNS Dinámico Gratuito)**

Registrarse en https://www.duckdns.org y crear un subdominio:

```
greenhouse-tuusuario.duckdns.org → 10.0.0.104
```

Instalar actualizador de IP:

```bash
# Crear script de actualización
echo 'echo url="https://www.duckdns.org/update?domains=greenhouse-tuusuario&token=TU_TOKEN&ip=" | curl -k -o ~/duckdns.log -K -' > ~/duckdns.sh
chmod +x ~/duckdns.sh

# Añadir a crontab (cada 5 minutos)
crontab -e
# Añadir: */5 * * * * ~/duckdns.sh
```

**3. Configurar Nginx como Reverse Proxy**

Crear `/etc/nginx/sites-available/greenhouse`:

```nginx
server {
    listen 80;
    server_name greenhouse-tuusuario.duckdns.org;

    location / {
        proxy_pass http://10.0.0.104:80;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        
        # WebSocket support
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        
        # Timeouts
        proxy_connect_timeout 60s;
        proxy_send_timeout 60s;
        proxy_read_timeout 60s;
    }
}
```

Activar configuración:

```bash
sudo ln -s /etc/nginx/sites-available/greenhouse /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl reload nginx
```

**4. Obtener Certificado Let's Encrypt**

```bash
sudo certbot --nginx -d greenhouse-tuusuario.duckdns.org
```

Certbot configurará automáticamente HTTPS y la renovación automática.

**5. Abrir Puertos en Router**

Configurar port forwarding en tu router:

```
Puerto Externo: 443 (HTTPS)
Puerto Interno: 80
IP Destino: 10.0.0.104 (o IP del servidor Nginx)
```

**6. Acceder**

```
https://greenhouse-tuusuario.duckdns.org/
```

---

### Opción 3: mDNS + Certificado Local (Simplificado)

#### Ventajas
- ✅ Sin servicios externos
- ✅ Nombre de dominio local (.local)
- ✅ Certificación mediante certificado raíz local

#### Desventajas
- ❌ Solo funciona en la red local
- ❌ Requiere instalar certificado raíz en cada dispositivo

#### Implementación

**1. Habilitar mDNS en ESP32**

En `src/system.cpp`:

```cpp
#include <ESPmDNS.h>

void SystemManager::setupMDNS() {
    if (MDNS.begin("greenhouse")) {
        DEBUG_PRINTLN(F("mDNS responder started: greenhouse.local"));
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("https", "tcp", 443);
    }
}
```

Llamar en `setup()`:

```cpp
void SystemManager::begin() {
    // ... código existente ...
    setupMDNS();
}
```

**2. Generar Certificado para greenhouse.local**

```bash
openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
  -keyout greenhouse_key.pem \
  -out greenhouse_cert.pem \
  -subj "/C=AR/ST=BuenosAires/L=CABA/O=Greenhouse/CN=greenhouse.local"
```

**3. Instalar Certificado Raíz**

En cada dispositivo que acceda, importar `greenhouse_cert.pem` como certificado de confianza:

- **Windows:** Doble clic → Instalar certificado → Almacén de certificados raíz de confianza
- **macOS:** Doble clic → Keychain Access → Marcar como "Siempre confiar"
- **Linux:** `sudo cp greenhouse_cert.pem /usr/local/share/ca-certificates/greenhouse.crt && sudo update-ca-certificates`
- **Android/iOS:** Enviar por email e instalar desde Ajustes > Seguridad

**4. Acceder**

```
https://greenhouse.local/
```

---

## 🔒 Recomendaciones de Seguridad

### Red Local (Solo acceso interno)
✅ **Opción 1** (Certificado autofirmado) es suficiente.

### Acceso Remoto Ocasional
✅ **Opción 2** (Reverse Proxy con Let's Encrypt) es la mejor opción.

### Red Corporativa/Múltiples Dispositivos
✅ **Opción 3** (mDNS + Certificado raíz local) simplifica la distribución.

---

## 📝 Configuración Adicional: HSTS y Headers de Seguridad

Si implementas HTTPS, añade headers de seguridad en `src/api.cpp`:

```cpp
void APIManager::setCORSHeaders(AsyncWebServerResponse* response) {
    // CORS existente
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    
    // Security headers (solo si HTTPS está habilitado)
    #ifdef ENABLE_HTTPS
    response->addHeader("Strict-Transport-Security", "max-age=31536000; includeSubDomains");
    response->addHeader("X-Content-Type-Options", "nosniff");
    response->addHeader("X-Frame-Options", "DENY");
    response->addHeader("X-XSS-Protection", "1; mode=block");
    response->addHeader("Referrer-Policy", "no-referrer");
    #endif
}
```

---

## 🚀 Script de Conversión Automatizada

Para convertir certificados PEM a código C++:

```python
#!/usr/bin/env python3
# cert_to_cpp.py

import sys

def pem_to_der_array(input_file, output_name):
    with open(input_file, 'rb') as f:
        data = f.read()
    
    # Convertir a array C
    hex_data = ', '.join(f'0x{byte:02x}' for byte in data)
    
    # Dividir en líneas de 12 bytes
    hex_lines = []
    hex_list = hex_data.split(', ')
    for i in range(0, len(hex_list), 12):
        hex_lines.append(', '.join(hex_list[i:i+12]))
    
    formatted_data = ',\n  '.join(hex_lines)
    
    print(f'const uint8_t {output_name}[] PROGMEM = {{')
    print(f'  {formatted_data}')
    print('};')
    print(f'const size_t {output_name}_len = sizeof({output_name});')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f'Uso: {sys.argv[0]} <input.der> <output_name>')
        sys.exit(1)
    
    pem_to_der_array(sys.argv[1], sys.argv[2])
```

Uso:

```bash
python3 cert_to_cpp.py greenhouse_cert.der server_cert_der > certificates.h
python3 cert_to_cpp.py greenhouse_key.der server_key_der >> certificates.h
```

---

## 📚 Referencias

- **ESP32 HTTPS Server:** https://github.com/fhessel/esp32_https_server
- **Let's Encrypt:** https://letsencrypt.org/
- **DuckDNS:** https://www.duckdns.org/
- **mDNS:** https://en.wikipedia.org/wiki/Multicast_DNS

---

## ⚠️ Notas Importantes

1. **Rendimiento:** HTTPS consume más CPU y RAM (~30KB adicionales). Monitorea el heap.
2. **Puerto 443:** Si el ESP32 usa el puerto 443, el puerto 80 debe redirigir o deshabilitarse.
3. **Firewall:** Asegúrate de que el puerto 443 esté abierto si usas reverse proxy.
4. **Renovación:** Con Let's Encrypt, los certificados vencen cada 90 días (Certbot renueva automáticamente).
5. **Mixed Content:** Si tienes recursos externos HTTP, el navegador los bloqueará en HTTPS.

---

**Última actualización:** 13 de octubre de 2025  
**Estado:** Documentación completa - Implementación pendiente según opción elegida
