
# Copilot Instructions for Greenhouse Project


## Arquitectura General
- **Firmware (ESP32, PlatformIO):** En `src/` y `include/`, controla sensores/relays, WiFi, OTA y comunica con el backend por HTTP/WebSocket. Configuración sensible en `include/secrets.h`.
- **Backend (Node.js, Socket.IO):** En `backend-websocket-update/`, gestiona API REST y WebSocket, autenticación, logs, reglas y estados. Configuración en `.env`.
- **Frontend (React, Vite):** En `greenhouse-dashboard/`, muestra datos en tiempo real, control manual/automático de relays, logs y reglas. Usa WebSocket y actualizaciones automáticas.
- **Nginx:** Proxy reverso para `/greenhouse/socket.io/` y dashboard web.

## Hardware y Configuración
- ESP32 NodeMCU-32S, DHT11, 2x sensores de humedad de suelo, módulo de 4 relays.
- Pines y sensores definidos en `include/sensors.h` y `include/relays.h`.
- WiFi 2.4GHz, OTA por puerto 3232, hostname `ESP32_GREENHOUSE_01.local`.

## Seguridad y Autenticación
- Todas las comunicaciones ESP32 ↔️ Backend requieren Bearer Token. El token debe coincidir entre `include/secrets.h` y `backend-websocket-update/.env`.
- Cambia el token por defecto y usa tokens largos/aleatorios. No publiques tokens reales.
- El backend desconecta clientes no autenticados y aplica backoff exponencial ante fallos.
- Verifica autenticación en logs con: `pm2 logs greenhouse-backend | grep auth`.
- Protege rutas HTTP y eventos WebSocket con middleware y validación.

## Flujos de Trabajo Esenciales
- **Firmware:** Compila y sube con `pio run --target upload` o `ota-upload.sh`. Edita credenciales en `include/secrets.h`.
- **Backend:** Instala con `npm install`, inicia con `npm start`. Configura `.env` antes de iniciar. Usa PM2 para gestión.
- **Frontend:** Instala con `npm install`, ejecuta con `npm run dev` o `npm run build` para producción.
- **Debug:** Usa `pio device monitor` para ESP32 y revisa logs backend con `pm2 logs`.
- **Factory Reset:** Mantén GPIO 25 a GND durante boot para borrar configuración.

## Comunicación y Eventos
- **WebSocket:** Backend (`server.js`) y frontend (`src/services/websocket.js`, `src/hooks/useWebSocket.js`) gestionan eventos como `sensor:new`, `relay:changed`, `rule:created`, `log:new`.
- **API REST:** Endpoints en `routes/api.js`, modelos en `models/` (`SensorReading.js`, `RelayState.js`).
- **Nginx:** Proxy para `/greenhouse/socket.io/` y dashboard web (`nginx.conf`).

## Validaciones y Automatización
- Validación automática de sensores (rangos, errores consecutivos, anomalías).
- Circuit breaker para fallos repetidos.
- Sistema de reglas: umbral, horario, combinadas. Modifica relays según sensores y lógica definida.

## Troubleshooting y Recuperación
- Verifica tokens y archivos `.env`/`secrets.h` si hay errores de autenticación.
- Revisa puertos y logs de Nginx (`/var/log/nginx/*.log`) ante problemas de conexión.
- Usa el monitor serial y logs para depurar errores de firmware.
- Factory reset y rollback disponibles para firmware y backend.

## Ejemplos de Extensión
- Para agregar sensores: actualiza `sensors.h` (firmware), `SensorReading.js` (backend), y componentes del frontend.
- Para nuevos eventos WebSocket: modifica `server.js`, `websocket.js` y `useWebSocket.js`.
- Para nuevas reglas: edita lógica en backend y frontend.

## Buenas Prácticas y Calidad
- No publiques tokens ni archivos sensibles.
- Mantén `.env` y `secrets.h` fuera del control de versiones.
- Rota tokens periódicamente y usa HTTPS/WSS en producción.
- Usa constantes en `config.h`, comentarios Doxygen, validación exhaustiva y logs estructurados.

## Referencias Rápidas
- Compilación: `pio run`, Upload: `pio run --target upload`, OTA: `ota-upload.sh`
- Backend: `npm install`, `npm start`, PM2 para logs y restart.
- Frontend: `npm install`, `npm run dev`, `npm run build`
- API: `GET /api/sensors`, `POST /api/relays/{id}`
- WebSocket: eventos `sensor:new`, `relay:updated`, `log:new`


## Seguridad y Autenticación
- Todas las comunicaciones ESP32 ↔️ Backend requieren Bearer Token. El token debe coincidir entre `include/secrets.h` y `backend-websocket-update/.env`.
- Cambia el token por defecto y usa tokens largos/aleatorios. No publiques tokens reales.
- El backend desconecta clientes no autenticados y aplica backoff exponencial ante fallos.
- Verifica autenticación en logs con: `pm2 logs greenhouse-backend | grep auth`.

## Flujos de Trabajo Esenciales
- **Firmware:** Compila y sube con `pio run --target upload`. Edita credenciales en `include/secrets.h`.
- **Backend:** Instala con `npm install`, inicia con `npm start`. Configura `.env` antes de iniciar.
- **Frontend:** Instala con `npm install`, ejecuta con `npm run dev` o `npm run build` para producción.
- **OTA Updates:** Usa `ota-upload.sh` y scripts relacionados.
- **Debug:** Usa `pio device monitor` para ESP32 y revisa logs backend con `pm2 logs`.

## Comunicación y Eventos
- **WebSocket:** Backend (`server.js`) y frontend (`src/services/websocket.js`, `src/hooks/useWebSocket.js`) gestionan eventos como `sensor:new`, `relay:changed`, `rule:created`.
- **Nginx:** Proxy para `/greenhouse/socket.io/` y configuración en `nginx.conf`.
- **API REST:** Endpoints en `routes/api.js`, modelos en `models/`.

## Troubleshooting
- Verifica tokens y archivos `.env`/`secrets.h` si hay errores de autenticación.
- Revisa puertos y logs de Nginx (`/var/log/nginx/*.log`) ante problemas de conexión.
- Usa el monitor serial y logs para depurar errores de firmware.

## Ejemplos de Extensión
- Para agregar sensores: actualiza `sensors.h` (firmware), `SensorReading.js` (backend), y componentes del frontend.
- Para nuevos eventos WebSocket: modifica `server.js`, `websocket.js` y `useWebSocket.js`.

## Buenas Prácticas
- No publiques tokens ni archivos sensibles.
- Mantén `.env` y `secrets.h` fuera del control de versiones.
- Rota tokens periódicamente y usa HTTPS/WSS en producción.

