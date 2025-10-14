// embedded_assets.cpp
// Implements restoration of essential web dashboard assets from flash.

#include <LittleFS.h>
#include "embedded_assets.h"

#ifdef ENABLE_EMBEDDED_ASSET_RESTORE

// NOTE: Raw string literals used for readability. If flash size becomes an issue,
// these can be minified or optionally gzip-compressed and expanded at runtime.

// index.html
static const char INDEX_HTML[] PROGMEM = R"====(<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Greenhouse Control System v3.0</title>
    <link rel="stylesheet" href="/style.css">
</head>
<body>
    <div class="container">
        <!-- Header with Status -->
        <header class="main-header">
            <div class="header-info">
                <h1>🌱 Greenhouse Control System</h1>
                <div class="connection-banner" id="connectionBanner">
                    <span id="connectionStatus">🔄 Connecting...</span>
                    <span id="lastUpdate">Last update: Never</span>
                </div>
            </div>
            <div class="system-overview">
                <div class="overview-card" id="systemHealth">
                    <h3>System Health</h3>
                    <div class="health-metrics">
                        <span class="metric">Uptime: <strong id="systemUptime">--</strong></span>
                        <span class="metric">Memory: <strong id="systemMemory">--</strong></span>
                        <span class="metric">WiFi: <strong id="wifiStatus">--</strong></span>
                    </div>
                </div>
            </div>
        </header>

        <!-- Alert Panel -->
        <div class="alert-panel" id="alertPanel" style="display: none;">
            <div class="alert-header">
                <h3>⚠️ System Alerts</h3>
                <div>
                    <button class="alert-close" id="alertMinimizeBtn" title="Minimize">−</button>
                    <button class="alert-close" id="alertCloseBtn" title="Close">×</button>
                </div>
            </div>
            <div class="alert-list" id="alertList"></div>
        </div>

        <!-- Main Dashboard Grid -->
        <main class="dashboard-grid">
            <!-- Environmental Sensors Section -->
            <section class="dashboard-section">
                <div class="section-header">
                    <h2>🌡️ Environmental Sensors</h2>
                    <div class="sensor-controls">
                        <button class="btn btn-secondary" id="pauseBtn">
                            <span id="pauseText">⏸️ Pause</span>
                        </button>
                        <button class="btn btn-secondary" id="exportDataBtn">📊 Export Data</button>
                    </div>
                </div>
                
                <!-- Sensor Status Cards -->
                <div class="sensor-grid">
                    <div class="sensor-card" id="dht-card">
                        <div class="sensor-header">
                            <h3>DHT11 Sensor</h3>
                            <div class="sensor-status" id="dht-status">🔴</div>
                        </div>
                        <div class="sensor-values">
                            <div class="value-item">
                                <span class="label">Temperature:</span>
                                <span class="value" id="temperature">--°C</span>
                            </div>
                            <div class="value-item">
                                <span class="label">Humidity:</span>
                                <span class="value" id="humidity">--%</span>
                            </div>
                        </div>
                    </div>

                    <div class="sensor-card" id="soil-card">
                        <div class="sensor-header">
                            <h3>Soil Moisture</h3>
                            <div class="sensor-status" id="soil-status">🔴</div>
                        </div>
                        <div class="sensor-values">
                            <div class="value-item">
                                <span class="label">Sensor 1:</span>
                                <span class="value" id="soil1">--%</span>
                            </div>
                            <div class="value-item">
                                <span class="label">Sensor 2:</span>
                                <span class="value" id="soil2">--%</span>
                            </div>
                        </div>
                    </div>

                    <div class="sensor-card" id="temp-sensors-card">
                        <div class="sensor-header">
                            <h3>Temperature Sensors</h3>
                            <div class="sensor-status" id="temp-sensors-status">�</div>
                        </div>
                        <div class="sensor-values">
                            <div class="value-item">
                                <span class="label">External 1:</span>
                                <span class="value" id="temp1">--°C</span>
                            </div>
                            <div class="value-item">
                                <span class="label">External 2:</span>
                                <span class="value" id="temp2">--°C</span>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- Charts Section -->
                <div class="charts-container">
                    <div class="chart-wrapper">
                        <h3>Temperature & Humidity Trends</h3>
                        <div class="chart-controls">
                            <label>
                                <input type="checkbox" id="showTemp" checked> Temperature
                            </label>
                            <label>
                                <input type="checkbox" id="showHumidity" checked> Humidity
                            </label>
                        </div>
                        <canvas id="envChart" width="800" height="300"></canvas>
                    </div>
                    
                    <div class="chart-wrapper">
                        <h3>Soil Moisture Levels</h3>
                        <canvas id="soilChart" width="800" height="300"></canvas>
                    </div>
                </div>

                <!-- Statistics Panel -->
                <div class="stats-panel">
                    <h3>� Environmental Statistics</h3>
                    <div class="stats-grid">
                        <div class="stat-item">
                            <span class="stat-label">Temp Range:</span>
                            <span class="stat-value" id="tempRange">-- to --°C</span>
                        </div>
                        <div class="stat-item">
                            <span class="stat-label">Temp Average:</span>
                            <span class="stat-value" id="tempAvg">--°C</span>
                        </div>
                        <div class="stat-item">
                            <span class="stat-label">Humidity Range:</span>
                            <span class="stat-value" id="humidityRange">-- to --%</span>
                        </div>
                        <div class="stat-item">
                            <span class="stat-label">Humidity Average:</span>
                            <span class="stat-value" id="humidityAvg">--%</span>
                        </div>
                    </div>
                </div>
            </section>

            <!-- Relay Control Section -->
            <section class="dashboard-section" id="relay-section">
                <div class="section-header">
                    <h2>🔌 Relay Control System</h2>
                    <div class="relay-controls">
                        <button class="btn btn-warning" id="authBtn">🔐 Authenticate</button>
                        <button class="btn btn-secondary" style="display:none;" id="refreshRelays">🔄 Refresh</button>
                    </div>
                </div>

                <!-- Authentication required notice -->
                <div class="auth-notice" id="authNotice">
                    <div class="notice-content">
                        <h3>🔒 Authentication Required</h3>
                        <p>Relay control requires API authentication. Click "Authenticate" to enter your token.</p>
                    </div>
                </div>

                <!-- Relay Grid (Hidden until authenticated) -->
                <div class="relay-grid" id="relayGrid" style="display: none;">
                    <!-- Relay cards will be populated dynamically -->
                </div>

                <!-- Relay Usage Statistics -->
                <div class="relay-stats" id="relayStats" style="display: none;">
                    <h3>📊 Usage Statistics</h3>
                    <div class="usage-grid">
                        <!-- Usage stats will be populated dynamically -->
                    </div>
                </div>
            </section>

            <!-- System Information Section -->
            <section class="dashboard-section">
                <div class="section-header">
                    <h2>💻 System Information</h2>
                    <button class="btn btn-secondary" id="refreshSystemBtn">🔄 Refresh</button>
                </div>

                <div class="system-grid">
                    <div class="system-card">
                        <h3>Memory Usage</h3>
                        <div class="memory-info">
                            <div class="memory-bar">
                                <div class="memory-used" id="memoryBar"></div>
                            </div>
                            <div class="memory-details">
                                <span>Free: <strong id="freeHeap">--</strong></span>
                                <span>Fragmentation: <strong id="fragmentation">--%</strong></span>
                            </div>
                        </div>
                    </div>

                    <div class="system-card">
                        <h3>Network Status</h3>
                        <div class="network-info">
                            <div class="network-item">
                                <span>Status:</span>
                                <span id="networkStatus">--</span>
                            </div>
                            <div class="network-item">
                                <span>Signal:</span>
                                <span id="signalStrength">-- dBm</span>
                            </div>
                            <div class="network-item">
                                <span>Reconnects:</span>
                                <span id="reconnectCount">--</span>
                            </div>
                        </div>
                    </div>

                    <div class="system-card">
                        <h3>Performance</h3>
                        <div class="performance-info">
                            <div class="perf-item">
                                <span>Loop Average:</span>
                                <span id="loopAvg">-- μs</span>
                            </div>
                            <div class="perf-item">
                                <span>System State:</span>
                                <span id="systemState">--</span>
                            </div>
                        </div>
                    </div>
                </div>
            </section>
        </main>

        <!-- Footer -->
        <footer class="main-footer">
            <div class="footer-info">
                <span>Greenhouse Control System v3.0</span>
                <span>Last data: <span id="lastDataTime">Never</span></span>
            </div>
        </footer>
    </div>

    <!-- Authentication Modal -->
    <div class="modal-overlay" id="authModal" style="display: none;">
        <div class="modal-content">
            <div class="modal-header">
                <h3>🔐 API Authentication</h3>
                <button class="modal-close" id="modalCloseBtn">×</button>
            </div>
            <div class="modal-body">
                <p>Enter your API token to access relay controls and advanced features:</p>
                <input type="password" id="authToken" placeholder="Enter API token" class="auth-input">
                <div class="auth-buttons">
                    <button class="btn btn-primary" id="authenticateBtn">Authenticate</button>
                    <button class="btn btn-secondary" id="cancelAuthBtn">Cancel</button>
                </div>
                <div class="auth-help">
                    <small>Token is stored locally and encrypted. Check your system configuration for the API token.</small>
                </div>
            </div>
        </div>
    </div>

    <!-- Tooltip -->
    <div class="tooltip" id="tooltip"></div>

    <script src="/script.js"></script>
</body>
</html>)====";

// style.css (truncated or full as needed - include essential styling)
static const char STYLE_CSS[] PROGMEM = R"====(/* Greenhouse Control System v3.0 Styles */
:root { --primary: #667eea; --primary-dark: #764ba2; --secondary: #4ecdc4; --success: #52c41a; --warning: #faad14; --danger: #ff4d4f; --bg: #0f1419; --bg-alt: #1a1f24; --bg-card: #242932; --text: #ffffff; --text-secondary: #8c9ba5; --border: #2a2f36; --accent: #3fbf7f; --radius: 8px; }
body { background: var(--bg); color:#fff; font-family: 'Segoe UI',sans-serif; }
.container { max-width:1600px; margin:0 auto; padding:20px; }
.main-header { background: linear-gradient(135deg,var(--primary) 0%,var(--primary-dark) 100%); padding:20px; border-radius:8px; }
.dashboard-section { background: #242932; border:1px solid #2a2f36; border-radius:8px; padding:20px; margin-bottom:20px; }
.sensor-grid, .system-grid, .relay-grid { display:grid; gap:15px; }
.sensor-card, .system-card { background:#1a1f24; border:1px solid #2a2f36; border-radius:8px; padding:15px; }
.alert-panel { position:fixed; top:80px; right:20px; background:rgba(0,0,0,0.7); border-radius:20px; max-width:280px; max-height:40px; overflow:hidden; }
.toast-container { position:fixed; top:20px; right:20px; z-index:9999; }
.toast { background:rgba(26,30,37,0.95); color:#fff; padding:12px 16px; margin-bottom:8px; border-radius:8px; border-left:4px solid var(--primary); }
@media (max-width:768px){ .container { padding:10px; } }
)====";

// script.js embebido eliminado (se sirve únicamente desde LittleFS)
// Eliminado script.js embebido para simplificar: ahora debe existir en LittleFS
// static const char SCRIPT_JS[] PROGMEM = R"====(... fallback removed ...)====";

// logs.html
static const char LOGS_HTML[] PROGMEM = R"====(<!doctype html><html><head><meta charset="utf-8"><title>Logs</title><link rel="stylesheet" href="/style.css"></head><body><h1>Device Logs</h1><pre id="logs">No logs available in this build.</pre><script src="/script.js"></script></body></html>)====";

// settings.html
static const char SETTINGS_HTML[] PROGMEM = R"====(<!doctype html><html><head><meta charset="utf-8"><title>Settings</title><link rel="stylesheet" href="/style.css"></head><body><h1>Device Settings</h1><p>Configure WiFi and device options here (UI coming soon).</p></body></html>)====";

static EmbeddedAsset EMBEDDED_ASSETS[] = {
    { "/index.html", INDEX_HTML, sizeof(INDEX_HTML) - 1 },
    { "/style.css", STYLE_CSS, sizeof(STYLE_CSS) - 1 },
    { "/logs.html", LOGS_HTML, sizeof(LOGS_HTML) - 1 },
    { "/settings.html", SETTINGS_HTML, sizeof(SETTINGS_HTML) - 1 }
};

static constexpr size_t EMBEDDED_ASSET_COUNT = sizeof(EMBEDDED_ASSETS) / sizeof(EmbeddedAsset);

bool writeFileIfNeeded(const EmbeddedAsset &asset, bool force, Stream *logOut) {
    // Mode from config.h
    const int mode = EMBEDDED_ASSETS_OVERWRITE_MODE; // 0,1,2

    // Hard-protect full dashboard script: if /script.js already exists in FS, never overwrite
    // This avoids the minimal embedded fallback replacing the richer version after uploads or size changes.
    if (strcmp(asset.path, "/script.js") == 0) {
        if (LittleFS.exists("/script.js")) {
            File existing = LittleFS.open("/script.js", "r");
            if (existing) {
                size_t sz = existing.size();
                existing.close();
                if (sz != asset.size && logOut) {
                    logOut->println(String(F("[ASSETS] Preserving existing /script.js despite mismatch (fs=")) + sz + F(", fw=" ) + asset.size + F(")"));
                }
            }
            return true; // always preserve existing full script
        } else {
            // Only create fallback if missing entirely
            File f = LittleFS.open("/script.js", "w");
            if (!f) { if (logOut) logOut->println(F("[ASSETS] Failed to create fallback /script.js")); return false; }
            f.write(reinterpret_cast<const uint8_t*>(asset.content), asset.size);
            f.close();
            if (logOut) logOut->println(String(F("[ASSETS] Installed fallback /script.js (")) + asset.size + F(" bytes)"));
            return true;
        }
    }

    if (!LittleFS.exists(asset.path) || force) {
        File f = LittleFS.open(asset.path, "w");
        if (!f) { if (logOut) logOut->println(String(F("[ASSETS] Failed to open ")) + asset.path + F(" for write")); return false; }
        f.write(reinterpret_cast<const uint8_t*>(asset.content), asset.size);
        f.close();
        if (logOut) logOut->println(String(F("[ASSETS] Restored ")) + asset.path + F(" (" ) + asset.size + F(" bytes)"));
        return true;
    }
    // If exists, verify size; if mismatch, overwrite
    File existing = LittleFS.open(asset.path, "r");
    if (!existing) { return writeFileIfNeeded(asset, true, logOut); }
    size_t sz = existing.size();
    existing.close();
    if (sz != asset.size) {
        if (mode == 2) {
            if (logOut) logOut->println(String(F("[ASSETS] Size mismatch for ")) + asset.path + F(" (fs=") + sz + F(", fw=") + asset.size + F(") -> refreshing"));
            return writeFileIfNeeded(asset, true, logOut);
        } else if (mode == 1) {
            if (logOut) logOut->println(String(F("[ASSETS] Mismatch ignored (mode=1) for ")) + asset.path);
            return true; // keep existing
        } else { // mode 0
            if (logOut) logOut->println(String(F("[ASSETS] Mismatch & preserve existing (mode=0) ")) + asset.path);
            return true; // keep existing
        }
    }
    return true; // already ok
}

bool restoreEmbeddedAssets(bool force, Stream *logOut) {
    if (logOut) logOut->println(String(F("[ASSETS] Restoration start (force=")) + (force?"true":"false") + ")");
    bool ok = true;
    for (size_t i = 0; i < EMBEDDED_ASSET_COUNT; ++i) {
        if (!writeFileIfNeeded(EMBEDDED_ASSETS[i], force, logOut)) {
            ok = false;
        }
    }
    if (logOut) logOut->println(ok ? F("[ASSETS] All assets present") : F("[ASSETS] Some assets failed"));
    return ok;
}

#endif // ENABLE_EMBEDDED_ASSET_RESTORE
