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
- Validation in `backend/app.js` (fatal error if missing or <32 chars)

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

**Backend Development** (`backend/`):
- Start: `npm start` (reads `.env`, validates ESP32_AUTH_TOKEN)
- Health check: `curl http://localhost:3000/health`
- Architecture: Modular structure with handlers/, middleware/, lib/, models/, config/, sockets/
  - `app.js` (55 lines) - Entry point & orchestration only
  - `handlers/` - HTTP endpoints (health.js, frontend.js)
  - `middleware/` - Request processing (cors-security.js, rateLimiter.js)
  - `lib/` - Utilities (server-setup.js, health-stats.js, **ruleEngine.js**)
  - `sockets/socketHandlers.js` - All WebSocket event handlers
  - `models/` - MongoDB schemas (SensorReading, RelayState, Rule, SystemLog)
  - `config/database.js` - MongoDB connection
- **Rule Engine** (`lib/ruleEngine.js`):
  - `evaluateSensorRules(sensorReading)` - Triggered on new sensor data
    - Evaluates all enabled sensor-based rules
    - Conditions: temperature, humidity, soil_moisture with operators (>, <, >=, <=, ==)
    - Actions: turn_on, turn_off via relay_id (0=Lights, 1=Fan, 2=Pump, 3=Heater)
    - Saves RelayState to MongoDB with mode='rule'
  - `evaluateTimeRules()` - Runs every 60 seconds
    - Evaluates time-based rules at scheduled times
    - Supports specific days of week (0=Sunday, 6=Saturday)
    - Automatic relay control with full audit trail
- Rate limiter: `middleware/rateLimiter.js` (120 events/min per socket)
- Logs: `docker compose logs -f app` (when in containers)

**Frontend Development** (`frontend/`):
- Dev server: `npm run dev` (port 5173, auto-connects to localhost:3000)
- Build: `npm run build` → `dist/` (static files served by Express)
- WebSocket service: `src/services/websocket.js` (environment-aware URL detection)
- Hooks: `src/hooks/useWebSocket.js` (8 custom React hooks)
- Components use hooks exclusively (no HTTP calls)

**Docker & Deployment**:
- Local dev: `docker compose up -d` (MongoDB + backend with hot-reload volumes)
- MongoDB 4.4 for CPU compatibility (no AVX requirement)

## Production Architecture: Deployment & Nginx Proxy

### The Setup

```
┌─────────────────────────────────────────────────────────────┐
│ Internet (HTTPS: https://reimon.dev/greenhouse/*)           │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│ External Nginx Server (Port 80/443)                         │
│ ⚠️  NOT controlled by this project - managed externally      │
│ Proxy rule: /greenhouse/* → http://127.0.0.1:3000/*        │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│ Docker Container: Express + Socket.IO (localhost:3000)      │
│ - Serves frontend build artifacts                           │
│ - Serves /socket.io/ WebSocket endpoint                     │
│ - Health check endpoint                                     │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│ MongoDB (Docker internal, port 27017, not exposed)          │
└─────────────────────────────────────────────────────────────┘
```

### How the Nginx Proxy Works

The Nginx server is **NOT** controlled by this project. An external team manages `/etc/nginx/` configuration.

**Request flow:**
```
Browser:  https://reimon.dev/greenhouse/api/relay/states
            ↓
External Nginx (config not in repo):
  location /greenhouse/ {
      proxy_pass http://127.0.0.1:3000/;  # Strips /greenhouse/ prefix
      proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
      proxy_set_header X-Forwarded-Proto $scheme;
      # WebSocket Upgrade headers automatically handled
  }
            ↓
Our Backend (Express at localhost:3000):
  Receives: GET /api/relay/states
  (without the /greenhouse/ prefix)
```

### Why This Works

1. **Frontend built for `/greenhouse/` path**
   - `vite.config.js` has `base: '/greenhouse/'`
   - All assets: `/greenhouse/assets/...`
   - React Router handles paths within `/greenhouse/`

2. **Nginx strips the prefix**
   - Request `/greenhouse/foo` → `/foo` (proxied to localhost:3000)

3. **Express handles all requests**
   - Static files at localhost:3000
   - WebSocket at localhost:3000/socket.io/
   - Health endpoint at localhost:3000/health

4. **WebSocket through the proxy**
   - Client: `https://reimon.dev/greenhouse/socket.io/`
   - Nginx: `http://127.0.0.1:3000/socket.io/` (with Upgrade headers)
   - Result: Real-time bidirectional communication

### URL Configuration by Environment

| Environment | URL | Path |
|-------------|-----|------|
| **Dev** | http://localhost:3000 | / (root) |
| **Prod** | https://reimon.dev | /greenhouse/ |

Frontend auto-detects via `import.meta.env.DEV`:
```javascript
// src/services/websocket.js
const wsUrl = import.meta.env.DEV 
  ? 'http://localhost:3000' 
  : 'https://reimon.dev/greenhouse'
```

## Automated Deployment (GitHub Actions)

**Setup** (One-time):
1. Generate SSH key on VPS: `ssh-keygen -t ed25519 -f ~/.ssh/github_deploy -N ""`
2. Add to authorized_keys: `cat ~/.ssh/github_deploy.pub >> ~/.ssh/authorized_keys`
3. Create GitHub Secrets:
   - `VPS_HOST`: `reimon.dev`
   - `VPS_USER`: `rei`
   - `VPS_SSH_KEY`: (content of `~/.ssh/github_deploy`)
   - `VPS_SSH_PORT`: `22` (or your custom port)
   - `ENV_PRODUCTION`: (content of `.env.production`)
   - `ESP32_AUTH_TOKEN`: (32+ char token for backup)

**Workflow** (`.github/workflows/deploy.yml`):
1. **Environment Setup**: Creates `.env` files from GitHub Secrets in 3 locations:
   - `.env.production` (root)
   - `backend/.env`
   - `docker/.env.production`

2. **Backup**: Creates incremental backup excluding:
   - `node_modules/` (reinstalled by Docker)
   - `dist/` (regenerated by build)
   - `logs/` (not needed)
   - `.git/` (not needed)

3. **Git Pull**: Updates code with `--ff-only` flag (prevents accidental merges)

4. **Validation**: Checks `docker-compose.prod.yml` syntax

5. **Restart Services** (without MongoDB):
   - Backend container (`app`)
   - Frontend/Nginx container (`web`)

6. **Health Checks**: Verifies `https://reimon.dev/health` with:
   - 5 retry attempts (3 seconds between retries)
   - If all fail → **automatic rollback** (restore backup + restart)

7. **Cleanup**: Removes temporary backups

**Duration**: ~2-5 minutes per deploy

**Logs**:
- GitHub Actions: Real-time logs in Actions tab
- VPS: `docker compose -f docker/docker-compose.prod.yml logs -f app`
- Saved: `$PROJECT_DIR/logs/deploy_TIMESTAMP.log`

**Protected Files** (preserved across deploys):
- `.env` files (restored from GitHub Secrets)
- `esp32-firmware/include/secrets.h` (never modified, backed up for recovery)
- MongoDB data (service never restarted during deploy)

**Rollback Triggers**:
- Health check failure after 5 retries
- Docker configuration validation error
- Automatic restoration of previous code + `.env` files + `secrets.h`

**Monitoring**:
- Push to GitHub → Actions tab shows deployment progress
- Check health endpoint: `curl https://reimon.dev/health`
- For issues, review GitHub Actions logs (detailed step-by-step output)

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
- Frontend connects to `https://reimon.dev` proxy URL, not direct backend
- Nginx (external team manages) proxies `/greenhouse/*` → `localhost:3000/`
- WebSocket Upgrade headers automatically handled by Nginx

### Climate Data in Sensor Handler
- `sensor:data` WebSocket handler fetches climate from Open-Meteo API
- Cache: 5-minute TTL to avoid API spam
- If humidity >= 95%: emits `sensor:storm` event (broadcast to all clients)
- Climate data available to rule engine for conditional automation

## Useful References

- **`esp32-firmware/include/config.h`**: Central firmware config (debug, timings, sensor sensitivity)
- **`backend/app.js`**: Entry point & orchestration (55 lines)
- **`backend/lib/ruleEngine.js`**: Sensor and time-based rule evaluation engine
- **`backend/sockets/socketHandlers.js`**: All WebSocket event handlers
- **`frontend/src/hooks/useWebSocket.js`**: Frontend real-time data subscriptions

## Example: Test WebSocket Connection

```bash
# Monitor backend for incoming sensor:data events
docker compose logs -f app | grep "sensor:new"

# Check health status
curl http://localhost:3000/health

# Trigger relay command via browser console
# webSocketService.emitToServer('relay:command', {relay_id: 0, state: true, mode: 'manual'})
```

## Quick Commands Reference

```bash
# Firmware
cd esp32-firmware
pio run                                      # Build
pio run --target upload                      # Upload
pio device monitor --baud 115200             # Serial monitor

# Backend
cd backend
npm start                                    # Start server
curl http://localhost:3000/health            # Health check

# Frontend
cd frontend
npm run dev                                  # Dev server (port 5173)
npm run build                                # Production build

# Docker
docker compose up -d                         # Start all services
docker compose logs -f app                   # View backend logs
docker compose down -v                       # Stop and remove volumes

# Deployment
# Push to main → GitHub Actions automatically deploys to VPS
# Monitor: https://github.com/repo/actions (see workflow logs)
```

## Current System State

- **100% WebSocket**: No HTTP polling or REST endpoints
- **Latency**: 50-100ms real-time updates
- **Scalability**: Single persistent connection per client
- **Build**: 0 errors
- **Project Structure**: Clean and organized

## Key Reminders

- **Secrets**: Never commit `.env`, `.env.production`, or `secrets.h`
- **Auth Token**: Must be 32+ chars, identical in firmware and backend
- **WebSocket Only**: No HTTP fallback or REST endpoints
- **Rate Limiting**: 120 events/minute per socket
- **GitHub Secrets**: Use for sensitive data in CI/CD (VPS_HOST, VPS_SSH_KEY, ENV_PRODUCTION)
- **Health Endpoint**: Always check after deployment (`curl https://reimon.dev/greenhouse/health`)
- **TTL Data**: Sensor data auto-deletes after 30 days; check timestamps when querying old data
- **Environment**: Dev uses localhost:3000, production uses https://reimon.dev/greenhouse/
- **Nginx**: External team manages it; we proxy through /greenhouse/ path (NOT controlled by this project)
