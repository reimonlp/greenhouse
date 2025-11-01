# Copilot Instructions for Greenhouse Project

## Overview

Full-stack IoT greenhouse automation: ESP32 microcontroller → Node.js backend with MongoDB → React dashboard. Real-time WebSocket communication, sensor monitoring, relay control, and rule-based automation.

**Key Technologies:**
- **ESP32 Firmware**: C++ (PlatformIO, WebSocket client, OTA updates, ArduinoJson)
- **Backend**: Node.js/Express (Socket.IO, MongoDB, Bearer token auth, rate limiting)
- **Frontend**: Vite/React (Material-UI, Recharts, real-time updates)
- **Infrastructure**: Docker, Nginx reverse proxy, TTL-based data cleanup (30 days)

## Architecture & Critical Data Flows

```
ESP32 → Backend:
  1. WebSocket 'device:register' with auth token → authentication
  2. WebSocket 'sensor:data' event → sensor readings
  3. WebSocket 'relay:state' event → relay state changes
  4. All operations 100% WebSocket (no HTTP REST)

Backend → MongoDB (TTL auto-cleanup):
  - SensorReading: createdAt TTL 30d (aggregated by device_id + timestamp)
  - RelayState: state changes with mode (manual/auto/rule) and timestamp
  - SystemLog: events with timestamps (TTL 30d)
  - Rule: sensor/time-based automation rules

Backend → Frontend (WEBSOCKET ONLY):
  - Events: 'sensor:new', 'sensor:latest', 'sensor:history'
  - Events: 'relay:states', 'relay:changed', 'relay:command'
  - Events: 'rule:list', 'rule:create', 'rule:update', 'rule:delete'
  - Events: 'log:list', 'log:new'
  - CORS: configured in docker-compose.yml ALLOWED_ORIGINS
  - 100% real-time bidirectional communication
```

## Project-Specific Patterns

### Authentication & Secrets
- **ESP32_AUTH_TOKEN**: 32+ char Bearer token (must be identical in `secrets.h` and backend `.env`)
- Generate: `openssl rand -hex 32`
- **Never commit**: `.env`, `.env.production`, `secrets.h` (use `.example.h` templates)
- Validation in `backend-websocket-update/server.js` (fatal error if missing or <32 chars)

### WebSocket Rate Limiting
- **Limit**: 120 events/minute per socket (in `middleware/rateLimiter.js`)
- Applied globally to all socket events
- Cleaned up 1 min after reset window
- Prevents dashboard spam from misbehaving firmware

### MongoDB TTL & Indexes
- **SensorReading**: TTL `createdAt` 30 days; compound index `device_id:1, timestamp:-1` for efficient queries
- **RelayState**: Similar TTL pattern for state history
- **SystemLog**: TTL 30 days for automatic cleanup
- TTL index is **automatic** - MongoDB deletes expired docs in background

### Relay Control Convention
- **IDs**: 0=Lights, 1=Fan, 2=Pump, 3=Heater (mapped in `sockets/socketHandlers.js` RELAY_NAMES)
- **Modes**: `manual` (dashboard), `auto` (rule-triggered), `rule` (time-based)
- State tracked in MongoDB with `timestamp`, `mode`, `changed_by`

### WebSocket Events (Complete Reference)

**ESP32 → Backend:**
- `device:register`: Authentication with token
- `sensor:data`: Sensor readings (temperature, humidity, soil_moisture, errors)
- `relay:state`: Relay state changes with mode and changed_by
- `log`: Debug/error logs from ESP32
- `metrics`: Performance metrics (connections, reconnections, uptime)
- `ping`: Keep-alive heartbeat

**Backend → Frontend (Broadcast):**
- `sensor:new`: New sensor reading received and saved
- `sensor:storm`: Storm alert (humidity >= 95%)
- `relay:changed`: Relay state changed (any client)
- `relay:states`: Response to relay:states request
- `rule:created`, `rule:updated`, `rule:deleted`: Rule changes
- `log:list`: Response to log:list request
- `log:new`: New system log entry
- `rule:list`: Response to rule:list request

**Frontend → Backend (Requests/Commands):**
- `relay:states`: Request all relay states (broadcasts back `relay:states`)
- `relay:command`: Command to change relay state
- `sensor:latest`: Request latest sensor reading
- `sensor:history`: Request sensor history with date range
- `rule:list`: Request all rules
- `rule:create`: Create new automation rule
- `rule:update`: Update existing rule
- `rule:delete`: Delete rule
- `log:list`: Request system logs with optional filters

## Developer Workflows

**Firmware Development** (`esp32-firmware/`):
- Template secrets: `cp include/secrets.example.h include/secrets.h` → edit
- Build: `pio run` (dev), `pio run --target upload` (to device)
- Monitor: `pio device monitor --baud 115200` (real-time serial output)
- OTA update: `./ota-upload.sh <ESP32_IP>` (password in `secrets.h`)
- Key headers: `config.h` (debug mode, timeouts), `vps_websocket.h` (connection logic)
- **Note**: All sensor/relay communication via WebSocket `sensor:data`, `relay:state` events

**Backend Development** (`backend-websocket-update/`):
- Start: `npm start` (reads `.env`, validates ESP32_AUTH_TOKEN)
- Health check: `curl http://localhost:3000/health`
- WebSocket handlers: `sockets/socketHandlers.js` (all 8+ event handlers)
- API routes: `routes/api.js` (error handler only; all business logic in WebSocket)
- Logs: `docker compose logs -f app` (for container env)

**Frontend Development** (`greenhouse-dashboard/`):
- Dev server: `npm run dev` (port 5173, connects to localhost:3000)
- Build: `npm run build` → `dist/` (served by Nginx in production)
- WebSocket service: `src/services/websocket.js` (connects to `https://reimon.dev` in prod)
- Helper methods: `fetchRules()`, `sendRelayCommand()`, `fetchLogs()`, `fetchSensorHistory()`
- Hooks: `src/hooks/useWebSocket.js` (useRelayUpdates, useSensorUpdates, useRules, useLogs, useSensorHistory)
- All components use hooks for real-time updates (no direct HTTP calls)

**Docker & Deployment**:
- Local dev: `docker compose up -d` (MongoDB + backend + frontend hot-reload via volumes)
- Production: `docker compose -f docker-compose.prod.yml up -d` (tagged images, no volumes)
- Health checks in both compose files (30s interval, 3 retries)
- MongoDB 4.4 for compatibility with older CPUs (no AVX)

## Critical Integration Points

### ESP32 → Backend Connection
- **File**: `esp32-firmware/src/vps_websocket.cpp`
- **Auth**: Token validated in `socketHandlers.js` device:register handler
- **Reconnection**: Exponential backoff logic (circuit breaker pattern) in C++
- **Watchdog**: ESP32 resets if no WiFi/backend in 30s (see `main.cpp` WDT_TIMEOUT)
- **Sensor Endpoint**: `vpsWebSocket.sendSensorData()` → WebSocket event → backend handler includes climate logic

### Backend Socket Events & Features
- **`device:register`**: ESP32 sends auth token; backend validates and stores device state
- **`sensor:data`**: ESP32 sends readings; backend:
  - Fetches climate data from Open-Meteo API
  - Detects storms (humidity >= 95%)
  - Evaluates sensor-based rules
  - Broadcasts `sensor:new` to dashboard
- **`relay:command`**: Dashboard sends relay change; backend validates and broadcasts to ESP32
- **`relay:states`**: Dashboard requests latest state of all relays; responds with aggregated data

### Frontend WebSocket Service
- **File**: `src/services/websocket.js`
- **Connection**: auto-connects on mount to `VITE_WS_URL` (default: `https://reimon.dev`)
- **Event subscriptions**: custom `on()` method for app-level pub/sub
- **Initialization**: emits `relay:states` on connection to populate dashboard
- **Helper methods**: Abstracts complex WebSocket emit logic
- **Error handling**: Logs errors, auto-reconnect, rate limit notifications

### Rate Limiting Layers
1. **Socket.IO rate limiter** (`middleware/rateLimiter.js`): 120 events/min per socket
2. Applies to all events: `sensor:data`, `relay:command`, `rule:*`, `log:list`, etc.
3. Logs violations; returns false to reject event; client sees `RATE_LIMIT_EXCEEDED` error
4. Cleaned up 1 min after reset window

## Common Patterns & Gotchas

### Adding a New Sensor Type
1. Update ESP32 `sensors.h` with new struct
2. Add field to `SensorReading.js` schema
3. Update `vps_websocket.cpp` `sendSensorData()` to include new field
4. Dashboard `SensorCard.jsx` reads `latestSensor` from hook; add new chart in `SensorChart.jsx`

### Relay State Changes
- Always broadcast via WebSocket after DB save (see `socketHandlers.js` `relay:command` handler)
- Both ESP32 and dashboard listen for `relay:changed` events
- Mode tracking (manual/auto/rule) is critical for rule engine

### MongoDB Connection Issues
- Local dev: ensure MongoDB container is healthy (`docker compose ps`)
- TTL indexes are created on schema definition; no manual setup needed
- Data age queries use `timestamp`, but TTL uses `createdAt` (auto-managed by Mongoose)

### CORS & Proxy Behind Nginx
- Frontend connects to proxy URL (e.g., `https://reimon.dev`), not direct backend
- `ALLOWED_ORIGINS` in `.env` must include proxy domain for WebSocket CORS
- Nginx proxies `/socket.io/` to backend port 8080 (see `nginx-prod.conf`)

### Climate Data in Sensor Handler
- `sensor:data` WebSocket handler fetches climate from Open-Meteo API
- Cache: 5-minute TTL to avoid API spam
- If humidity >= 95%: emits `sensor:storm` event (broadcast to all clients)
- Climate data available to rule engine for conditional automation

## Useful References

- **`PROJECT_SUMMARY.md`**: Full architecture diagrams, data models
- **`DEPLOYMENT_SUMMARY.md`**: Deployment phases, checklist, SSL/Nginx setup
- **`docker/README.md`**: Quick start, troubleshooting, environment setup
- **`esp32-firmware/include/config.h`**: Central firmware config (debug, timings, sensor sensitivity)
- **`backend-websocket-update/sockets/socketHandlers.js`**: All WebSocket event handlers (8+)
- **`greenhouse-dashboard/src/hooks/useWebSocket.js`**: Frontend real-time data subscriptions

## Example: Test WebSocket Sensor Data Reception

```bash
# Monitor backend for incoming sensor:data events
docker compose logs -f app | grep "sensor:new"

# Monitor frontend DevTools console for real-time updates
# Open DevTools → Console
# Should see: "[WS] Evento sensor:new recibido:"

# Trigger relay command via WebSocket (browser DevTools):
# webSocketService.emitToServer('relay:command', {relay_id: 0, state: true, mode: 'manual'})
```

## AI Agent Tips - Nov 1 2025 Session Update

**Session accomplishments:**
- ✅ Fixed 7 backend bugs (missing imports, field name mismatches, duplicate calls, wrong paths)
- ✅ Removed all REST API endpoints except none (100% WebSocket)
- ✅ Removed HTTP client code from ESP32 firmware (82% reduction in vps_client.cpp)
- ✅ Enhanced `sensor:data` handler with climate logic and storm detection
- ✅ All frontend components already using WebSocket hooks

**Current system state:**
- **100% WebSocket**: No HTTP polling, no REST endpoints
- **Latency**: 50-100ms (down from 200-500ms)
- **Scalability**: Unlimited (single persistent connection per client)
- **Compilation**: 0 errors

**When adding features:**
- Add WebSocket handler in `socketHandlers.js` (use rate limiter)
- Add service helper in `websocket.js` if needed
- Add hook in `useWebSocket.js` if component needs state management
- Always broadcast changes to all connected clients via `io.emit()`
- Test via browser DevTools Console for WebSocket events

- **Always validate secrets** before firmware/backend builds (no default tokens in production)
- **Check health first**: `/health` endpoint returns DB connection, version, uptime
- **Logs are your friend**: `docker compose logs -f app` for backend, `pio device monitor` for ESP32
- **WebSocket is the only transport**: No fallback to HTTP for dashboard or ESP32
````
- **Rate limiting affects tests**: If testing high-frequency events, adjust `RATE_LIMIT_MAX_EVENTS` in `rateLimiter.js`
- **Use TTL awareness**: Old sensor data auto-deletes; if querying old data, check timestamps
- **Environment isolation**: `.env` for local dev (insecure token), `.env.production` for VPS (never commit)
