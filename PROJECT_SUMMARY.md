# Greenhouse Project - Comprehensive Overview

## Project Description

**ESP32 Greenhouse Monitoring & Control System** - A complete IoT solution for automated greenhouse management with real-time sensor monitoring, relay control, and web-based dashboard.

The project consists of three main components:
1. **ESP32 Firmware** - Embedded C++ running on ESP32 microcontroller
2. **Backend Server** - Node.js with Express, MongoDB, and Socket.IO
3. **Frontend Dashboard** - React with Vite and Material-UI

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    Internet / Home Network                       │
└─────────────────────────────────────────────────────────────────┘
                              │
                    ┌─────────┴────────┐
                    │                  │
            ┌──────▼──────┐    ┌──────▼──────┐
            │ Browser/App │    │ ESP32       │
            │ (Dashboard) │    │ Firmware    │
            └──────┬──────┘    └──────┬──────┘
                   │                  │
                   └──────────┬───────┘
                              │
                    ┌─────────▼────────┐
                    │  Nginx Proxy     │
                    │  Port 3000/8080  │
                    └─────────┬────────┘
                              │
        ┌─────────────────────┴──────────────────────┐
        │                                            │
    ┌───▼──────────┐                        ┌───────▼──────┐
    │ Node.js API  │────────────────────────│ Socket.IO    │
    │ Express      │ (HTTP + WebSocket)     │ (Real-time)  │
    │ Port 8080    │                        │              │
    └───┬──────────┘                        └───────┬──────┘
        │                                           │
        └───────────────────┬──────────────────────┘
                            │
                    ┌───────▼──────────┐
                    │  MongoDB         │
                    │  Port 27017      │
                    │                  │
                    │ Collections:     │
                    │ - SensorReadings │
                    │ - RelayStates    │
                    │ - Rules          │
                    │ - SystemLogs     │
                    └──────────────────┘
```

---

## Directory Structure

```
greenhouse/
├── .github/
│   └── copilot-instructions.md          # AI assistant guidelines
│
├── esp32-firmware/                      # ESP32 Firmware (PlatformIO)
│   ├── include/
│   │   ├── config.h                     # Central configuration (debug, timeouts, thresholds)
│   │   ├── secrets.h                    # Credentials (WiFi, tokens, OTA password)
│   │   ├── secrets.example.h            # Template for secrets.h
│   │   ├── pins.h                       # GPIO pin assignments
│   │   ├── sensors.h                    # DHT11, soil moisture sensor definitions
│   │   ├── relays.h                     # 4x relay definitions (lights, fan, pump, heater)
│   │   ├── vps_client.h                 # HTTP client for backend communication
│   │   ├── vps_config.h                 # VPS-specific configuration
│   │   ├── vps_ota.h                    # Over-The-Air update configuration
│   │   └── vps_websocket.h              # WebSocket client for real-time communication
│   │
│   ├── src/
│   │   ├── main.cpp                     # ESP32 entry point and main loop
│   │   ├── sensors_simple.cpp           # Sensor reading implementation
│   │   ├── relays_simple.cpp            # Relay control implementation
│   │   ├── vps_client.cpp               # HTTP client implementation
│   │   └── vps_websocket.cpp            # WebSocket client implementation
│   │
│   ├── platformio.ini                   # PlatformIO configuration
│   ├── partitions.csv                   # Flash memory partitioning
│   └── ota-upload.sh                    # OTA update script
│
├── backend-websocket-update/            # Node.js Backend
│   ├── config/
│   │   └── database.js                  # MongoDB connection setup
│   │
│   ├── middleware/
│   │   └── rateLimiter.js               # WebSocket rate limiting
│   │
│   ├── models/
│   │   ├── SensorReading.js             # Sensor data schema (TTL 30 days)
│   │   ├── RelayState.js                # Relay state history
│   │   ├── Rule.js                      # Automation rules (sensor/time-based)
│   │   └── SystemLog.js                 # System logs (TTL 30 days)
│   │
│   ├── routes/
│   │   ├── api.js                       # REST API endpoints
│   │   └── health.js                    # Health check endpoint
│   │
│   ├── sockets/
│   │   └── socketHandlers.js            # WebSocket event handlers
│   │
│   ├── server.js                        # Express + Socket.IO server setup
│   ├── package.json                     # Dependencies
│   ├── .env                             # Configuration (not in git)
│   ├── .env.example                     # Configuration template
│   ├── test-auth.sh                     # Authentication test script
│   ├── test-health.sh                   # Health check test script
│   ├── test-rate-limit.sh               # Rate limiting test script
│   └── update-websocket.sh              # Deployment update script
│
├── greenhouse-dashboard/                # React Frontend
│   ├── src/
│   │   ├── components/
│   │   │   ├── Dashboard.jsx            # Main dashboard layout
│   │   │   ├── SensorCard.jsx           # Sensor display component
│   │   │   ├── SensorChart.jsx          # Temperature/humidity charts
│   │   │   ├── RelayControl.jsx         # Relay on/off controls
│   │   │   ├── RuleManager.jsx          # Rule creation/management
│   │   │   └── LogViewer.jsx            # System log viewer
│   │   │
│   │   ├── hooks/
│   │   │   └── useWebSocket.js          # WebSocket connection hook
│   │   │
│   │   ├── services/
│   │   │   ├── websocket.js             # WebSocket client setup
│   │   │   ├── api.js                   # REST API calls
│   │   │   └── config/api.js            # API endpoint configuration
│   │   │
│   │   ├── App.jsx                      # Root component with theme
│   │   ├── main.jsx                     # React entry point
│   │   └── index.html                   # HTML template
│   │
│   ├── package.json                     # Dependencies (React, Axios, Recharts)
│   ├── vite.config.js                   # Vite configuration
│   ├── eslint.config.js                 # ESLint rules
│   ├── .env.example                     # Environment template
│   └── .env.production                  # Production environment
│
├── docker/                              # Docker Deployment
│   ├── Dockerfile                       # Multi-stage build
│   ├── docker-compose.yml               # Service orchestration
│   ├── nginx.conf                       # Nginx reverse proxy config
│   └── start.sh                         # Container startup script
│
├── .vscode/                             # VS Code configuration
│   ├── c_cpp_properties.json            # C++ IntelliSense
│   ├── settings.json                    # Editor settings
│   ├── tasks.json                       # Build tasks
│   ├── extensions.json                  # Recommended extensions
│   └── launch.json                      # Debug configuration
│
└── .clang-format, .clang-tidy           # C++ code style
```

---

## Technology Stack

### Hardware
- **MCU**: ESP32 (NodeMCU-32S variant)
- **Sensors**:
  - DHT11 (Temperature & Humidity)
  - Analog soil moisture sensors (2x)
- **Actuators**: 
  - 4x Relays (Lights, Ventilation, Irrigation, Heating)
- **Communication**: 
  - WiFi 2.4GHz
  - WebSocket (Socket.IO)

### Firmware (ESP32)
- **Language**: C++17
- **Framework**: Arduino with ESP-IDF
- **Build System**: PlatformIO
- **Libraries**:
  - ArduinoJson (JSON parsing)
  - DHT sensor library (sensor reading)
  - WebSockets (real-time communication)
  - ArduinoOTA (firmware updates)
  - NTPClient (time synchronization)

### Backend
- **Runtime**: Node.js 20
- **Framework**: Express.js
- **Database**: MongoDB 7
- **Real-time**: Socket.IO 4.8
- **Security**: Helmet, CORS
- **Logging**: Morgan
- **Rate Limiting**: express-rate-limit

### Frontend
- **Framework**: React 19
- **Build Tool**: Vite 7
- **Styling**: Material-UI (MUI)
- **Charts**: Recharts
- **HTTP Client**: Axios
- **WebSocket**: Socket.IO Client

### Deployment
- **Containerization**: Docker & Docker Compose
- **Reverse Proxy**: Nginx
- **Environment**: Linux (Alpine)

---

## Key Features

### 1. Real-Time Monitoring
- **Sensor Data**: Temperature, Humidity, Soil Moisture
- **Live Updates**: WebSocket push to all connected clients
- **Data Persistence**: MongoDB with 30-day TTL indexes
- **Historical Charts**: Temperature and humidity trends

### 2. Relay Control
- **Manual Control**: Dashboard buttons to toggle relays
- **Auto Mode**: Rule-based automation
- **State Tracking**: Change history with timestamps
- **Names**: 
  - Relay 0: Lights (luces)
  - Relay 1: Ventilation Fan (ventilador)
  - Relay 2: Irrigation Pump (bomba)
  - Relay 3: Heater (calefactor)

### 3. Automation Rules
- **Sensor-based**: Temperature > X → Turn on Heater
- **Time-based**: 06:30 every Monday → Turn on Lights
- **Conditions**: `>, <, >=, <=, ==` operators
- **Smart Scheduling**: Per-day selection

### 4. Security
- **Authentication**: Bearer token on all ESP32-Backend communication
- **HTTPS/WSS**: Ready for production SSL
- **Rate Limiting**: 120 events/minute per WebSocket client
- **Validation**: Input validation on all endpoints
- **Token Rotation**: Supports periodic token updates

### 5. OTA Updates
- **Firmware Updates**: ESP32 can be updated over-the-air (port 3232)
- **Rollback Support**: Partition-based dual-boot
- **Password Protected**: OTA password in secrets.h

### 6. Health & Diagnostics
- **System Health Endpoint**: `/health` with uptime, memory, connections
- **Circuit Breaker**: Restarts if WebSocket disconnected > 5 times
- **Metrics Reporting**: Connection stats every 5 minutes
- **System Logs**: Audit trail of all actions

---

## Configuration Files

### ESP32 Configuration (`esp32-firmware/`)

**`secrets.h`** (MUST NOT be committed):
```cpp
#define WIFI_SSID "Your_WiFi_Name"
#define WIFI_PASSWORD "Your_WiFi_Password"
#define OTA_HOSTNAME "ESP32_GREENHOUSE_01"
#define OTA_PASSWORD "OTA_Update_Password"
#define VPS_SERVER "backend.example.com"
#define VPS_PORT 8080
#define VPS_USE_SSL false
```

**`config.h`** - Central configuration hub:
- Debug levels (0-4)
- Timeouts and intervals
- Sensor thresholds (DHT11 ranges)
- Rate limits
- Feature flags

**`pins.h`** - GPIO assignments:
- DHT11 sensor pin
- Relay pins
- Status LED pin

### Backend Configuration (`backend-websocket-update/`)

**`.env`** (MUST NOT be committed):
```env
PORT=8080
MONGODB_URI=mongodb://localhost:27017/greenhouse
ALLOWED_ORIGINS=http://localhost:5173,https://greenhouse.example.com
RATE_LIMIT_WINDOW_MS=900000
RATE_LIMIT_MAX_REQUESTS=10000
ESP32_AUTH_TOKEN=your_32_char_min_secure_token
```

### Frontend Configuration (`greenhouse-dashboard/`)

**`.env.production`**:
```env
VITE_API_URL=https://api.example.com
VITE_WS_URL=wss://api.example.com
```

---

## Communication Flows

### 1. Sensor Data Upload (ESP32 → Backend)
```
ESP32 (periodically, every 5s):
  └─ sensor:data {temperature, humidity, soil_moisture}
     ↓
Backend (socketHandlers.js):
  ├─ Save to MongoDB
  ├─ Evaluate rules
  └─ Broadcast to Dashboard via 'sensor:new'
     ↓
Dashboard (useWebSocket.js):
  ├─ Update component state
  ├─ Update charts
  └─ Display real-time values
```

### 2. Relay Command (Dashboard → ESP32)
```
Dashboard (RelayControl.jsx):
  └─ POST /api/relays/:id/state {state: true}
     ↓
Backend (api.js):
  ├─ Save state to MongoDB
  ├─ Emit 'relay:command' to esp32_devices room
  └─ Broadcast 'relay:changed' to all clients
     ↓
ESP32 (vps_websocket.cpp):
  ├─ Receive 'relay:command'
  ├─ Execute relay.setRelay(id, state)
  └─ Send 'relay:state' confirmation
```

### 3. Rule Evaluation
```
Sensor Data Received → Backend evaluates rules:
  ├─ IF temperature > 28 AND rule enabled
  ├─ THEN send relay:command {relay_id: 1, state: true}
  └─ Relay executes automatically

Time-based (every minute):
  ├─ Check scheduled rules (06:30, Monday)
  ├─ Compare current time
  └─ Execute matching rules
```

### 4. WebSocket Authentication
```
ESP32 Connection:
  1. device:register {device_id, auth_token, device_type}
  2. Backend validates auth_token against ESP32_AUTH_TOKEN
  3. If valid → device:auth_success + join 'esp32_devices' room
  4. If invalid → device:auth_failed + disconnect
```

---

## Data Models

### SensorReading (MongoDB)
```javascript
{
  _id: ObjectId,
  device_id: "ESP32_GREENHOUSE_01",
  temperature: 24.5,
  humidity: 65.3,
  soil_moisture: null,
  temp_errors: 0,
  humidity_errors: 0,
  timestamp: Date,
  createdAt: Date,        // TTL index: delete after 30 days
  updatedAt: Date
}
```

### RelayState (MongoDB)
```javascript
{
  _id: ObjectId,
  relay_id: 0,            // 0-3
  state: true,            // on/off
  mode: "manual",         // "manual" or "auto"
  changed_by: "user",     // "user", "rule", "system"
  timestamp: Date,
  createdAt: Date,        // TTL index: delete after 30 days
  updatedAt: Date
}
```

### Rule (MongoDB)
```javascript
{
  _id: ObjectId,
  relay_id: 1,
  enabled: true,
  rule_type: "sensor",    // "sensor" or "time"
  
  // For sensor rules:
  condition: {
    sensor: "temperature",  // "temperature", "humidity", "soil_moisture"
    operator: ">",
    threshold: 28
  },
  
  // For time rules:
  schedule: {
    time: "06:30",        // HH:MM format
    days: [1,2,3,4,5]     // 0=Sunday, 6=Saturday
  },
  
  action: "turn_on",      // "turn_on" or "turn_off"
  name: "Heat on hot day",
  created_at: Date,
  updated_at: Date
}
```

### SystemLog (MongoDB)
```javascript
{
  _id: ObjectId,
  level: "info",          // "debug", "info", "warning", "error"
  source: "esp32",        // "esp32", "api", "system", "rule_engine"
  message: "Relay 1 activated by rule",
  metadata: {
    relay_id: 1,
    rule_id: "507f1f77bcf86cd799439011"
  },
  timestamp: Date,
  createdAt: Date,        // TTL index: delete after 30 days
  updatedAt: Date
}
```

---

## API Endpoints

### REST API (HTTP)

#### Sensors
- `GET /api/sensors` - Get last 100 sensor readings
- `GET /api/sensors/latest` - Get most recent reading
- `POST /api/sensors` - Save new sensor reading (ESP32 only)

#### Relays
- `GET /api/relays/states` - Get current state of all relays
- `POST /api/relays/:id/state` - Change relay state (requires auth)

#### Rules
- `GET /api/rules` - Get all automation rules
- `POST /api/rules` - Create new rule
- `PUT /api/rules/:id` - Update rule
- `DELETE /api/rules/:id` - Delete rule

#### System
- `GET /health` - System health check
- `GET /api/logs` - Get system logs

### WebSocket Events

#### ESP32 → Backend
- `device:register` - ESP32 device connects
- `sensor:data` - Send sensor readings
- `relay:state` - Report relay state change
- `log` - Send log message
- `ping` - Keep-alive ping
- `metrics` - Send connection metrics

#### Backend → ESP32
- `device:auth_success` / `device:auth_failed` - Auth response
- `relay:command` - Command to toggle relay
- `connected` - Connection confirmation

#### Backend → Dashboard
- `sensor:new` - New sensor reading
- `relay:changed` - Relay state changed
- `rule:created` / `rule:updated` / `rule:deleted` - Rule changes
- `log:new` - New system log

---

## Security Considerations

### Authentication
1. **ESP32 Bearer Token**: 32+ character random string in both `secrets.h` and `.env`
2. **Token Validation**: Every WebSocket event and API call verified
3. **Rate Limiting**: 120 events/minute per socket, prevents brute force
4. **Circuit Breaker**: Auto-restart if > 5 failed connection attempts

### Best Practices
- ✅ Never commit `.env`, `.env.production`, or `secrets.h`
- ✅ Use HTTPS/WSS in production with valid certificates
- ✅ Rotate tokens periodically
- ✅ Keep firmware/backend updated
- ✅ Monitor logs for unauthorized access attempts
- ✅ Disable SerialDebug in production (config.h: DISABLE_SERIAL_OUTPUT)

### Production Deployment
```bash
# Check authentication logs
pm2 logs greenhouse-backend | grep auth

# Verify token match
cat backend-websocket-update/.env | grep ESP32_AUTH_TOKEN

# Monitor Nginx
tail -f /var/log/nginx/e
rror.log
```

---

## Deployment

### Docker Deployment (Recommended)
```bash
cd greenhouse/docker
docker-compose up --build
# Accessible at http://localhost:3000
```

### Manual Deployment

**Backend**:
```bash
cd backend-websocket-update
npm install
# Configure .env
npm start
# Or with PM2: pm2 start server.js --name "greenhouse-backend"
```

**Frontend**:
```bash
cd greenhouse-dashboard
npm install
npm run build
# Serve dist/ directory with Nginx
```

**ESP32 Firmware**:
```bash
cd esp32-firmware
# Configure secrets.h
pio run --target upload
# Or: ./ota-upload.sh for wireless update
```

---

## Troubleshooting

### ESP32 Won't Connect to WiFi
1. Verify WIFI_SSID and WIFI_PASSWORD in `secrets.h`
2. Check WiFi is 2.4GHz (ESP32 doesn't support 5GHz)
3. Monitor serial output: `pio device monitor`
4. Check signal strength: `WiFi.RSSI()` should be > -80 dBm

### Backend Authentication Fails
1. Verify ESP32_AUTH_TOKEN in `.env` matches `API_TOKEN` in `secrets.h`
2. Token must be minimum 32 characters
3. Check logs: `pm2 logs greenhouse-backend | grep auth`
4. Regenerate token: `openssl rand -hex 32`

### WebSocket Disconnects Frequently
1. Check rate limiting: 120 events/minute limit
2. Verify network stability
3. Monitor backend: `pm2 logs`
4. Check for proxy interference (Nginx logs)

### Sensor Readings Show NaN
1. Verify DHT11 pin in `pins.h`
2. Check DHT11 wiring (VCC, GND, DATA)
3. Ensure pull-up resistor (4.7kΩ) on DATA pin
4. Monitor DHT errors: error count increments in readings

### High Memory Usage
1. MongoDB TTL indexes delete old data after 30 days
2. Monitor with: `curl http://localhost:3000/health`
3. Check for stuck WebSocket connections
4. Restart backend: `pm2 restart greenhouse-backend`

---

## Development Workflow

### For Firmware Development
```bash
cd esp32-firmware
# Edit config.h, sensors.h, relays.h
# Update secrets.h with your credentials
pio run --target upload           # Upload via USB
# Or for OTA: ./ota-upload.sh [ESP32_IP_ADDRESS]

# Monitor
pio device monitor --baud 115200
```

### For Backend Development
```bash
cd backend-websocket-update
npm install
# Edit routes/api.js, models/*, sockets/socketHandlers.js
npm start                          # Development mode

# Test endpoints
./test-auth.sh
./test-health.sh
./test-rate-limit.sh
```

### For Frontend Development
```bash
cd greenhouse-dashboard
npm install
npm run dev                        # Vite dev server on :5173

# Edit components, services
npm run lint                       # Check code quality
npm run build                      # Production build
```

---

## Future Improvements

- [ ] Mobile app (React Native)
- [ ] Cloud sync (AWS/Firebase)
- [ ] Advanced ML-based rules
- [ ] Multi-device support (multiple greenhouses)
- [ ] Email/SMS alerts
- [ ] Data export (CSV/PDF)
- [ ] Advanced scheduling (complex conditions)
- [ ] Energy consumption tracking
- [ ] Backup & restore functionality

---

## References & Resources

- **PlatformIO Docs**: https://docs.platformio.org/
- **ESP32 Documentation**: https://docs.espressif.com/
- **Node.js Express**: https://expressjs.com/
- **Socket.IO**: https://socket.io/
- **MongoDB**: https://docs.mongodb.com/
- **React Documentation**: https://react.dev/
- **Material-UI**: https://mui.com/

---

## License & Attribution

This project uses open-source libraries:
- ArduinoJson by Benoit Blanchon
- DHT sensor library by Adafruit
- WebSockets library by Markus Sattler
- Socket.IO by Automattic
- Express.js by OpenJS Foundation
- MongoDB drivers by MongoDB Inc.

---

**Last Updated**: October 23, 2024  
**Project Status**: Active Development  
**Maintainer**: Reimon (reimon.dev)
