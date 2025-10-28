# Copilot Instructions for Greenhouse Project

## Overview

This repository is a full-stack IoT solution for automated greenhouse management, with:
- **esp32-firmware/**: C++ firmware for ESP32 (PlatformIO, ArduinoJson, OTA, WebSocket, NTPClient)
- **backend-websocket-update/**: Node.js Express server (MongoDB, Socket.IO, REST API, rate limiting, TTL)
- **greenhouse-dashboard/**: Vite/React dashboard (Material-UI, Recharts, live charts, relay control)
- **docker/**: Containerization, Nginx proxy, deployment scripts for local/VPS

See `PROJECT_SUMMARY.md` for a full architecture diagram and directory map.

## Architecture & Data Flow

- ESP32 → Backend: Sensor data via HTTP POST and WebSocket (`vps_websocket.cpp`)
- Backend → MongoDB: Sensor readings, relay states, rules, logs (TTL 30d)
- Backend → Frontend: Real-time updates via WebSocket (`sensor:new`, `relay:changed`)
- Frontend → Backend: Relay commands, rule management via REST API
- Nginx proxy handles SSL, routing, and public endpoints (see `docker/nginx-prod.conf`)
- See `DEPLOYMENT_SUMMARY.md` for deployment phases and architecture diagrams

## Developer Workflows

- **Firmware:**
  - Edit `esp32-firmware/include/secrets.h` (never commit real secrets)
  - Build/upload: `pio run --target upload` or `./ota-upload.sh [IP]`
  - Monitor: `pio device monitor --baud 115200`
- **Backend:**
  - Start: `npm start` or `docker compose up -d` (see health/test scripts)
  - Test: `./test-health.sh`, `./test-auth.sh`, `./test-rate-limit.sh`
- **Frontend:**
  - Start: `npm run dev` in `greenhouse-dashboard/`
  - Build: `npm run build` (served by Nginx in production)
- **Deployment:**
  - Local: `docker compose up -d`
  - VPS: `docker compose -f docker-compose.prod.yml up -d`
  - Nginx: configure `/etc/nginx/sites-available/greenhouse` and SSL (see deployment summary)
  - Always test local before VPS deploy

See `DEPLOYMENT_SUMMARY.md` for step-by-step deployment and validation commands.

## Project-Specific Conventions

- **Auth:** ESP32 uses 32+ char Bearer token in both `secrets.h` and backend `.env` (`ESP32_AUTH_TOKEN`)
- **Rate Limiting:** 120 events/minute per WebSocket client (see `rateLimiter.js`)
- **MongoDB TTL:** Sensor readings, relay states, logs auto-delete after 30 days
- **Relay IDs:** 0=Lights, 1=Fan, 2=Pump, 3=Heater
- **Rule Engine:** Sensor and time-based rules, with operators and per-day scheduling
- **Never commit:** `.env`, `.env.production`, `secrets.h`, tokens/passwords
- **Health:** `/health` endpoint, logs, and system metrics

See `PROJECT_SUMMARY.md` for data models, API endpoints, and communication flows.

## Integration Points

- **Nginx:** SSL, reverse proxy, public endpoints (see `docker/nginx-prod.conf`)
- **MongoDB:** Internal only in production, TTL indexes for auto-cleanup
- **OTA:** ESP32 supports OTA updates (port 3232, password in `secrets.h`)
- **System logs:** All actions logged, viewable via API and dashboard

See deployment summary for Nginx, SSL, and troubleshooting commands.

## Useful References

- `PROJECT_SUMMARY.md`: Architecture, models, API, flows
- `DEPLOYMENT_SUMMARY.md`: Deployment phases, commands, security
- `docker/README.md`: Quick start, troubleshooting
- `backend-websocket-update/routes/api.js`: REST API endpoints
- `greenhouse-dashboard/src/components/`: Main React components
- `esp32-firmware/src/`: Firmware logic

## Example: Simulate ESP32 Sensor POST
```bash
TOKEN="dev_token_minimum_32_characters_required_please_change_me_asap"
curl -X POST http://localhost:3000/api/sensors \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"device_id": "ESP32_TEST", "temperature": 25.5, "humidity": 60}'
```

## Tips for AI Agents

- Always use project scripts and checklists (`DEPLOYMENT_SUMMARY.md`) for builds, deploys, and troubleshooting
- Validate tokens and secrets before firmware/backend deploy
- Use health endpoints and logs for diagnostics
- Reference correct environment (local vs production) for endpoints and credentials
- Never expose secrets or tokens in code or commits

## Troubleshooting & Validation
- See `DEPLOYMENT_SUMMARY.md` and `PROJECT_SUMMARY.md` for common issues and validation steps (WiFi, auth, rate limiting, sensor errors, logs)
- Use provided curl commands and log checks for health and connectivity

## Security
- Always generate secure tokens (`openssl rand -hex 32`)
- Use HTTPS/WSS in production
- Rotate tokens periodically
- Monitor logs for unauthorized access
