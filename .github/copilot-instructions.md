# Copilot Instructions for Greenhouse Project

## Overview
This project manages a greenhouse system with three main components:
- **Firmware (ESP32, PlatformIO):** Located in `src/` and `include/`, controls sensors and relays, communicates with backend via HTTP/WebSocket.
- **Backend (Node.js, Socket.IO):** In `backend-websocket-update/`, provides WebSocket API, authentication, logging, and relay/sensor state management.
- **Frontend (React, Vite):** In `greenhouse-dashboard/`, displays real-time sensor/relay data, uses WebSocket for updates.

## Key Workflows
- **Build & Upload Firmware:**
  - Use PlatformIO: `pio run --target upload` (or VS Code task "Build and upload minimal firmware")
  - Edit secrets in `include/secrets.h` for device authentication
- **Backend Setup:**
  - Install dependencies: `npm install` in `backend-websocket-update/`
  - Start server: `npm start`
  - Auth token config: Edit `.env` and set `ESP32_AUTH_TOKEN` (see `README_AUTH_UPDATE.md`)
- **Frontend Setup:**
  - Install dependencies: `npm install` in `greenhouse-dashboard/`
  - Start dev server: `npm run dev`

## Communication Patterns
- **WebSocket (Socket.IO):**
  - Backend: `server.js` handles events for sensors, relays, and rules
  - Frontend: `src/services/websocket.js`, `src/hooks/useWebSocket.js` manage connection and event handling
  - Nginx proxies `/greenhouse/socket.io/` for WebSocket traffic
- **Authentication:**
  - All ESP32 ↔️ Backend communication uses Bearer token (see `README_AUTH_UPDATE.md`)
  - Token must match between backend `.env` and firmware `secrets.h`

## Project Conventions
- **Models:**
  - Backend models in `models/` (e.g., `SensorReading.js`, `RelayState.js`)
- **Routes:**
  - API endpoints in `routes/api.js`, health checks in `routes/health.js`
- **Middleware:**
  - Rate limiting in `middleware/rateLimiter.js`
- **Frontend Components:**
  - Dashboard UI in `src/components/`, charts in `SensorChart.jsx`, logs in `LogViewer.jsx`
- **Config Files:**
  - PlatformIO: `platformio.ini`
  - Nginx: `nginx.conf`, `backend-websocket-update/nginx-config-websocket`

## Testing & Debugging
- **Backend:**
  - Test scripts: `test-auth.sh`, `test-health.sh`, `test-rate-limit.sh` in `backend-websocket-update/`
  - Logs: Use `pm2 logs greenhouse-backend | grep -i auth` for authentication events
- **Firmware:**
  - Serial output for debugging
- **Frontend:**
  - Hot reload via Vite dev server

## Integration Points
- **OTA Updates:**
  - Scripts: `ota-upload.sh`, `update-websocket.sh`
- **Database:**
  - Config in `backend-websocket-update/config/database.js`

## Examples
- To add a new sensor type, update `sensors.h` (firmware), `SensorReading.js` (backend), and relevant frontend components.
- To add a new WebSocket event, update `server.js` (backend), `websocket.js` (frontend service), and `useWebSocket.js` (frontend hook).

---
For more details, see `README_AUTH_UPDATE.md` and `README_WEBSOCKET.md` in `backend-websocket-update/`.
