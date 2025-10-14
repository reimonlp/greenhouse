/**
 * Greenhouse Control System v2.0
 * Modern Modular JavaScript Architecture
 */

// UI boot log removed

// ============================================
// Configuration & Constants
// ============================================

const CONFIG = {
    API_BASE: window.location.origin,
    POLL_INTERVAL: 8000,
    CHART_MAX_POINTS: 50,
    STORAGE_KEY_TOKEN: 'gh_api_token',
    STORAGE_KEY_VIEW: 'gh_active_view'
};

// ============================================
// State Management
// ============================================

class AppState {
    constructor() {
        this.data = {
            sensors: { history: [] },
            relays: null,
            system: null,
            logs: [],
            apiToken: this.loadToken(),
            isAuthenticated: !!localStorage.getItem(CONFIG.STORAGE_KEY_TOKEN),
            currentView: this.loadView() || 'dashboard',
            isConnected: false,
            lastUpdate: null,
            logoutRequested: false
        };
        this.subscribers = [];
    }

    subscribe(callback) {
        this.subscribers.push(callback);
        return () => {
            this.subscribers = this.subscribers.filter(cb => cb !== callback);
        };
    }

    setState(updates) {
        // Merge updates
        const prevToken = this.data.apiToken;
        const next = { ...this.data, ...updates };
        // Derive auth strictly from presence of apiToken to avoid spurious flips
        next.isAuthenticated = !!next.apiToken;
        // Token change tracing removed
        this.data = next;
        this.subscribers.forEach(cb => cb(this.data));
    }

    getState() {
        return { ...this.data };
    }

    loadToken() {
        return localStorage.getItem(CONFIG.STORAGE_KEY_TOKEN);
    }

    saveToken(token) {
        // Sanitize token: trim and strip any non-ASCII/controls that can break headers
        const clean = (token || '').trim().replace(/[^\x20-\x7E]/g, '');
        localStorage.setItem(CONFIG.STORAGE_KEY_TOKEN, clean);
        this.setState({ apiToken: clean });
    }

    clearToken() {
        localStorage.removeItem(CONFIG.STORAGE_KEY_TOKEN);
    // clearToken trace removed
        this.setState({ apiToken: null });
    }

    loadView() {
        return localStorage.getItem(CONFIG.STORAGE_KEY_VIEW);
    }

    saveView(view) {
        localStorage.setItem(CONFIG.STORAGE_KEY_VIEW, view);
    }
}

const appState = new AppState();

// ============================================
// API Service
// ============================================

class APIService {
    async fetch(endpoint, options = {}) {
        const state = appState.getState();
        const method = (options.method || 'GET').toUpperCase();
        const headers = {
            'Accept': 'application/json',
            ...(state.apiToken && { 'Authorization': `Bearer ${state.apiToken}` }),
            ...options.headers
        };
        // Only set Content-Type when sending a body
        if (!headers['Content-Type'] && (method === 'POST' || method === 'PUT' || method === 'PATCH')) {
            headers['Content-Type'] = 'application/json';
        }

        try {
            const response = await fetch(`${CONFIG.API_BASE}${endpoint}`, {
                ...options,
                headers
            });

            if (!response.ok) {
                if (response.status === 401) {
                    // Don't clear token here; let callers decide.
                    // This avoids UI flapping to logged-out state on transient errors.
                    throw new Error(`HTTP 401`);
                }
                throw new Error(`HTTP ${response.status}`);
            }

            return await response.json();
        } catch (error) {
            console.error(`API Error (${endpoint}):`, error);
            throw error;
        }
    }

    // Public endpoints
    async getSensors() {
        return this.fetch('/api/sensors');
    }

    async getSystemHealth() {
        return this.fetch('/api/system/health');
    }

    // Protected endpoints
    async getRelays() {
        return this.fetch('/api/relays');
    }

    async setRelay(index, state) {
        return this.fetch('/api/relays/set', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: `relay=${index}&state=${state}`
        });
    }

    async setRelayMode(index, mode) {
        return this.fetch('/api/relays/mode', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: `relay=${index}&mode=${mode}`
        });
    }

    async getSystemStatus() {
        return this.fetch('/api/system/status');
    }

    async getFirmwareInfo() {
        return this.fetch('/api/firmware/info');
    }

    async getLogs(count = 50) {
        return this.fetch(`/api/logs?count=${count}`);
    }

    async clearLogs() {
        return this.fetch('/api/logs/clear', { method: 'DELETE' });
    }
}

const api = new APIService();

// ============================================
// UI Controller
// ============================================

class UIController {
    constructor() {
        this.elements = this.cacheElements();
        this.setupEventListeners();
        this.charts = {};
    }

    cacheElements() {
        return {
            // Sidebar
            sidebar: document.getElementById('sidebar'),
            sidebarToggle: document.getElementById('sidebarToggle'),
            navItems: document.querySelectorAll('.nav-item'),
            statusIndicator: document.getElementById('statusIndicator'),
            statusText: document.getElementById('statusText'),
            authBtn: document.getElementById('authBtn'),
            logoutBtn: document.getElementById('logoutBtn'),
            authStatus: document.getElementById('authStatus'),

            // Modals
            authModal: document.getElementById('authModal'),
            authTokenInput: document.getElementById('authToken'),
            modalCloseBtn: document.getElementById('modalCloseBtn'),
            cancelAuthBtn: document.getElementById('cancelAuthBtn'),
            authenticateBtn: document.getElementById('authenticateBtn'),

            // Alert Bar
            alertBar: document.getElementById('alertBar'),
            alertMessage: document.getElementById('alertMessage'),
            alertCloseBtn: document.getElementById('alertCloseBtn'),

            // Dashboard - Sensor cards
            temperature: document.getElementById('temperature'),
            humidity: document.getElementById('humidity'),
            soil1: document.getElementById('soil1'),
            soil2: document.getElementById('soil2'),
            temp1: document.getElementById('temp1'),
            temp2: document.getElementById('temp2'),
            dhtStatus: document.getElementById('dht-status'),
            soilStatus: document.getElementById('soil-status'),
            tempSensorsStatus: document.getElementById('temp-sensors-status'),
            
            // Statistics
            tempRange: document.getElementById('tempRange'),
            tempAvg: document.getElementById('tempAvg'),
            humidityRange: document.getElementById('humidityRange'),
            humidityAvg: document.getElementById('humidityAvg'),

            // Control
            authNotice: document.getElementById('authNotice'),
            relayGrid: document.getElementById('relayGrid'),
            relayStats: document.getElementById('relayStats'),

            // System
            freeHeap: document.getElementById('freeHeap'),
            fragmentation: document.getElementById('fragmentation'),
            memoryBar: document.getElementById('memoryBar'),
            networkStatus: document.getElementById('networkStatus'),
            signalStrength: document.getElementById('signalStrength'),
            reconnectCount: document.getElementById('reconnectCount'),
            loopAvg: document.getElementById('loopAvg'),
            systemState: document.getElementById('systemState'),
            lastDataTime: document.getElementById('lastDataTime'),

            // Charts
            envChart: document.getElementById('envChart'),
            soilChart: document.getElementById('soilChart')
        };
    }

    setupEventListeners() {
        // Sidebar toggle
        this.elements.sidebarToggle?.addEventListener('click', () => this.toggleSidebar());

        // Navigation
        this.elements.navItems.forEach(item => {
            item.addEventListener('click', () => this.switchView(item.dataset.view));
        });

        // Auth buttons (support duplicate IDs in layout)
        document.querySelectorAll('#authBtn, #authBtnControl')
            .forEach(btn => btn?.addEventListener('click', () => this.showAuthModal()));

        // Logout
        this.elements.logoutBtn?.addEventListener('click', () => {
            appState.setState({ logoutRequested: true });
            appState.clearToken();
            this.updateControlView();
            this.updateAuthStatus(false);
            this.showAlert('Sesión cerrada', 'info');
        });
        this.elements.modalCloseBtn?.addEventListener('click', () => this.hideAuthModal());
        this.elements.cancelAuthBtn?.addEventListener('click', () => this.hideAuthModal());
        this.elements.authenticateBtn?.addEventListener('click', () => this.authenticate());

        // Auth modal - Enter key
        this.elements.authTokenInput?.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') this.authenticate();
        });

    // Alert close
    this.elements.alertCloseBtn?.addEventListener('click', () => this.hideAlert());

    // Refresh buttons
    document.getElementById('refreshDashboard')?.addEventListener('click', () => this.refreshDashboard());
    document.getElementById('refreshSensors')?.addEventListener('click', () => this.refreshSensors());
    document.getElementById('refreshControl')?.addEventListener('click', () => this.refreshControl());
    // Wire actual button IDs present in HTML
    document.getElementById('refreshRelays')?.addEventListener('click', () => this.refreshControl());
    document.getElementById('refreshSystemBtn')?.addEventListener('click', () => this.refreshSystem());
    document.getElementById('refreshLogs')?.addEventListener('click', () => this.refreshLogs());

        // Export buttons
        document.getElementById('exportSensorData')?.addEventListener('click', () => this.exportSensorData());
        document.getElementById('exportLogs')?.addEventListener('click', () => this.exportLogs());

        // Clear logs
        document.getElementById('clearLogs')?.addEventListener('click', () => this.clearLogs());
    }

    toggleSidebar() {
        this.elements.sidebar?.classList.toggle('collapsed');
    }

    switchView(viewName) {
    // Switching view log removed
        
        // Update navigation
        this.elements.navItems.forEach(item => {
            item.classList.toggle('active', item.dataset.view === viewName);
        });

        // Scroll to the corresponding section
        // Since the HTML has a single-page layout, we'll scroll to relevant sections
        let targetElement = null;
        
        switch(viewName) {
            case 'dashboard':
                // Scroll to top (sensor cards)
                targetElement = document.querySelector('.sensor-grid');
                break;
            case 'sensors':
                // Scroll to charts section
                targetElement = document.querySelector('.charts-container');
                break;
            case 'control':
                // Scroll to relay section
                targetElement = document.getElementById('relay-section');
                break;
            case 'system':
                // Scroll to system info section
                targetElement = document.querySelector('.system-grid');
                break;
            case 'logs':
                // For now, show alert that logs view is not implemented
                this.showAlert('Logs view coming soon', 'info');
                break;
        }
        
        if (targetElement) {
            targetElement.scrollIntoView({ behavior: 'smooth', block: 'start' });
        }

        // Save preference
        appState.saveView(viewName);
        appState.setState({ currentView: viewName });
    }

    showAuthModal() {
        this.elements.authModal.style.display = 'flex';
        this.elements.authTokenInput.value = '';
        this.elements.authTokenInput.focus();
    }

    hideAuthModal() {
        this.elements.authModal.style.display = 'none';
    }

    async authenticate() {
        const token = this.elements.authTokenInput.value.trim();
        if (!token) {
            this.showAlert('Por favor ingresa un token válido', 'warning');
            return;
        }

        appState.saveToken(token);
        this.hideAuthModal();

        // 1) Verify token with a single guarded request
        let verifyData = null;
        try {
            verifyData = await api.getRelays();
        } catch (error) {
            // Only clear token if the verification call failed
            appState.clearToken();
            this.showAlert('Token inválido', 'danger');
            return;
        }

        // 2) Success path: render immediately and persist relays in state for grace mode
    // Auth success log removed
        this.showAlert('Autenticación exitosa', 'success');
        appState.setState({ logoutRequested: false, relays: verifyData });
        this.updateControlView();
        this.updateRelaysDisplay(verifyData);
        this.switchView('control');

        // 3) Fire a background refresh (non-fatal)
        this.updateRelays().catch(err => console.warn('[Auth] Post-auth relays refresh failed:', err?.message || err));
    }

    showAlert(message, type = 'info') {
        this.elements.alertMessage.textContent = message;
        this.elements.alertBar.style.display = 'flex';

        // Auto-hide after 5 seconds
        setTimeout(() => this.hideAlert(), 5000);
    }

    hideAlert() {
        this.elements.alertBar.style.display = 'none';
    }

    updateConnectionStatus(connected) {
        const indicator = this.elements.statusIndicator;
        const text = this.elements.statusText;

        if (connected) {
            indicator.classList.add('connected');
            indicator.classList.remove('disconnected');
            text.textContent = 'Conectado';
        } else {
            indicator.classList.remove('connected');
            indicator.classList.add('disconnected');
            text.textContent = 'Desconectado';
        }

        // NO llamar a setState aquí para evitar recursión infinita
        // El estado isConnected ya se actualiza en updateSensors/updateSystem
    }

    updateAuthStatus(authed) {
        if (this.elements.authStatus) {
            this.elements.authStatus.textContent = authed ? 'Auth OK' : 'No auth';
            this.elements.authStatus.style.color = authed ? 'var(--primary)' : 'var(--text-secondary)';
        }
        if (this.elements.logoutBtn) {
            this.elements.logoutBtn.style.display = authed ? 'inline-flex' : 'none';
        }
    }

    updateSensorDisplay(data) {
        // Temperature & Humidity
        if (this.elements.temperature) {
            this.elements.temperature.textContent = data.temperature ? `${data.temperature.toFixed(1)}°C` : '--°C';
        }
        if (this.elements.humidity) {
            this.elements.humidity.textContent = data.humidity ? `${data.humidity.toFixed(1)}%` : '--%';
        }
        
        // Soil moisture
        if (this.elements.soil1) {
            this.elements.soil1.textContent = data.soil_moisture_1 !== undefined ? `${data.soil_moisture_1.toFixed(1)}%` : '--%';
        }
        if (this.elements.soil2) {
            this.elements.soil2.textContent = data.soil_moisture_2 !== undefined ? `${data.soil_moisture_2.toFixed(1)}%` : '--%';
        }
        
        // External temperature sensors
        if (this.elements.temp1) {
            this.elements.temp1.textContent = data.temp_sensor_1 !== undefined ? `${data.temp_sensor_1.toFixed(1)}°C` : '--°C';
        }
        if (this.elements.temp2) {
            this.elements.temp2.textContent = data.temp_sensor_2 !== undefined ? `${data.temp_sensor_2.toFixed(1)}°C` : '--°C';
        }
        
        // Statistics
        if (data.statistics) {
            const stats = data.statistics;
            if (this.elements.tempRange) {
                this.elements.tempRange.textContent = `${stats.temp_min?.toFixed(1)} to ${stats.temp_max?.toFixed(1)}°C`;
            }
            if (this.elements.tempAvg) {
                this.elements.tempAvg.textContent = `${stats.temp_avg?.toFixed(1)}°C`;
            }
            if (this.elements.humidityRange) {
                this.elements.humidityRange.textContent = `${stats.humidity_min?.toFixed(0)} to ${stats.humidity_max?.toFixed(0)}%`;
            }
            if (this.elements.humidityAvg) {
                this.elements.humidityAvg.textContent = `${stats.humidity_avg?.toFixed(1)}%`;
            }
        }
        
        // Update last data time
        if (this.elements.lastDataTime) {
            this.elements.lastDataTime.textContent = new Date().toLocaleTimeString();
        }

        // Update status indicators
        this.updateSensorStatus(data);
    }

    updateSensorStatus(data) {
        const dhtValid = data.flags?.dht || (data.temperature !== null && data.humidity !== null);
        const soilValid = data.flags?.soil_complete || (data.soil_moisture_1 > 0 || data.soil_moisture_2 > 0);
        const extTempValid = data.flags?.ext_temps_complete || (data.temp_sensor_1 > 0 || data.temp_sensor_2 > 0);

        if (this.elements.dhtStatus) {
            this.elements.dhtStatus.textContent = dhtValid ? '🟢' : '🔴';
        }
        if (this.elements.soilStatus) {
            this.elements.soilStatus.textContent = soilValid ? '🟢' : '🔴';
        }
        if (this.elements.tempSensorsStatus) {
            this.elements.tempSensorsStatus.textContent = extTempValid ? '🟢' : '🔴';
        }
    }

    updateControlView() {
        const state = appState.getState();
        const authNotice = this.elements.authNotice || document.getElementById('authNotice');
        const relayGrid = this.elements.relayGrid || document.getElementById('relayGrid');
        const relayStats = this.elements.relayStats || document.getElementById('relayStats');
    const refreshRelaysBtn = document.getElementById('refreshRelays');

    const authed = !!state.apiToken;
    // Grace: if we have relays data, keep the view visible unless user explicitly logs out
    const relaysDataPresent = !!state.relays && Array.isArray(state.relays.relays);
    const showControl = state.logoutRequested ? authed : (authed || relaysDataPresent);

    if (authNotice) authNotice.style.display = showControl ? 'none' : 'block';
    if (relayGrid) relayGrid.style.display = showControl ? 'grid' : 'none';
    if (relayStats) relayStats.style.display = showControl ? 'grid' : 'none';
    if (refreshRelaysBtn) refreshRelaysBtn.style.display = showControl ? 'inline-flex' : 'none';
        // If authenticated and have relays in state, render immediately
        if (authed && appState.getState().relays) {
            this.updateRelaysDisplay(appState.getState().relays);
        }
        this.updateAuthStatus(authed);
    // updateControlView debug log removed
    }

    updateRelaysDisplay(data) {
        const grid = this.elements.relayGrid || document.getElementById('relayGrid');
        if (!grid || !data || !data.relays) return;

        const relayNames = ['Luces', 'Calefactor', 'Ventilador', 'Bomba'];
        const relayIcons = ['💡', '🔥', '🌀', '💧'];

        grid.innerHTML = data.relays.map((relay, index) => `
            <div class="card">
                <div class="card-header">
                    <h3 class="card-title">${relayIcons[index]} ${relayNames[index]}</h3>
                    <div class="relay-status ${relay.state ? 'active' : 'inactive'}">
                        ${relay.state ? 'ON' : 'OFF'}
                    </div>
                </div>
                <div class="card-body">
                    <div class="relay-controls">
                        <button class="btn-primary" onclick="toggleRelay(${index}, ${!relay.state})">
                            ${relay.state ? 'Apagar' : 'Encender'}
                        </button>
                        <select onchange="setRelayMode(${index}, this.value)" class="input-field">
                            <option value="manual" ${relay.mode === 'manual' ? 'selected' : ''}>Manual</option>
                            <option value="auto" ${relay.mode === 'auto' ? 'selected' : ''}>Auto</option>
                        </select>
                    </div>
                    <div class="info-list">
                        <div class="info-item">
                            <span class="info-label">Tiempo total ON</span>
                            <span class="info-value">${this.formatDuration(relay.total_on_time || 0)}</span>
                        </div>
                    </div>
                </div>
            </div>
        `).join('');

        // Ensure grid is visible in case toggle ran earlier
        grid.style.display = 'grid';
    // Rendered relays debug log removed

        // Update quick stats
        const activeCount = data.relays.filter(r => r.state).length;
        const quick = document.getElementById('quickRelays');
        if (quick) quick.textContent = `${activeCount}/4`;
    }

    updateSystemDisplay(data) {
        // System info
        if (this.elements.sysFreeHeap) {
            this.elements.sysFreeHeap.textContent = `${Math.round(data.free_heap / 1024)}KB`;
        }
        if (this.elements.sysFragmentation) {
            this.elements.sysFragmentation.textContent = data.fragmentation_ratio ? 
                `${(data.fragmentation_ratio * 100).toFixed(1)}%` : '--%';
        }
        if (this.elements.sysWiFiState) {
            this.elements.sysWiFiState.textContent = data.wifi ? 'Conectado' : 'Desconectado';
        }
        if (this.elements.sysRSSI) {
            this.elements.sysRSSI.textContent = data.rssi ? `${data.rssi} dBm` : '--';
        }
        if (this.elements.sysUptime) {
            this.elements.sysUptime.textContent = this.formatDuration(data.uptime);
        }
        // System info binds to actual IDs in HTML
        if (this.elements.freeHeap) {
            this.elements.freeHeap.textContent = data.free_heap ? `${Math.round(data.free_heap / 1024)}KB` : '--';
        }
        if (this.elements.fragmentation) {
            this.elements.fragmentation.textContent = (typeof data.fragmentation_ratio === 'number') ?
                `${(data.fragmentation_ratio * 100).toFixed(1)}%` : '--%';
        }
        if (this.elements.networkStatus) {
            this.elements.networkStatus.textContent = data.wifi ? 'Conectado' : 'Desconectado';
        }
        if (this.elements.signalStrength) {
            this.elements.signalStrength.textContent = (typeof data.rssi === 'number') ? `${data.rssi} dBm` : '--';
        }
        if (this.elements.reconnectCount) {
            const attempts = (typeof data.wifi_reconnect_attempts === 'number') ? data.wifi_reconnect_attempts : '--';
            this.elements.reconnectCount.textContent = attempts;
        }
        if (this.elements.loopAvg) {
            const loop = (typeof data.loop_avg_us === 'number') ? `${data.loop_avg_us} µs` : '--';
            this.elements.loopAvg.textContent = loop;
        }
        if (this.elements.systemState) {
            this.elements.systemState.textContent = data.state || '--';
        }
        // Optional memory bar fill if present and we can infer percent (best-effort)
        if (this.elements.memoryBar && typeof data.free_heap === 'number') {
            // Without total heap, just animate a pulse or leave as-is; skip to avoid misleading UI
        }
    }

    formatDuration(seconds) {
        if (!seconds) return '--';
        const days = Math.floor(seconds / 86400);
        const hours = Math.floor((seconds % 86400) / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);

        if (days > 0) return `${days}d ${hours}h`;
        if (hours > 0) return `${hours}h ${minutes}m`;
        return `${minutes}m`;
    }

    initChart(canvasId) {
        const canvas = document.getElementById(canvasId);
        if (!canvas) return null;

        const ctx = canvas.getContext('2d');
        return { canvas, ctx };
    }

    drawEnvironmentChart(sensorHistory) {
        const chart = this.charts.env || this.initChart('envChart');
        if (!chart) return;

        const { ctx, canvas } = chart;
        const width = canvas.width = canvas.offsetWidth;
        const height = canvas.height = canvas.offsetHeight;

        ctx.clearRect(0, 0, width, height);

        if (sensorHistory.length < 2) return;

        // Simple line chart implementation
        // (Simplified - you can integrate Chart.js for advanced features)
        ctx.strokeStyle = '#10b981';
        ctx.lineWidth = 2;
        ctx.beginPath();

        sensorHistory.forEach((point, index) => {
            const x = (index / sensorHistory.length) * width;
            const y = height - ((point.temperature / 50) * height);
            if (index === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        });

        ctx.stroke();
    }

    // Refresh methods
    async refreshDashboard() {
        await app.updateSensors();
        await app.updateSystem();
    }

    async refreshSensors() {
        await app.updateSensors();
    }

    async refreshControl() {
        await app.updateRelays();
    }

    async refreshSystem() {
        await app.updateSystem();
    }

    async refreshLogs() {
        await app.updateLogs();
    }

    exportSensorData() {
        const state = appState.getState();
        const data = JSON.stringify(state.sensors.history, null, 2);
        this.downloadFile('sensor-data.json', data);
    }

    exportLogs() {
        const state = appState.getState();
        const data = JSON.stringify(state.logs, null, 2);
        this.downloadFile('logs.json', data);
    }

    downloadFile(filename, content) {
        const blob = new Blob([content], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = filename;
        a.click();
        URL.revokeObjectURL(url);
    }

    async clearLogs() {
        if (!confirm('¿Estás seguro de que quieres limpiar todos los logs?')) return;

        try {
            await api.clearLogs();
            this.showAlert('Logs limpiados exitosamente', 'success');
            await this.refreshLogs();
        } catch (error) {
            this.showAlert('Error al limpiar logs', 'danger');
        }
    }
}

// ============================================
// Application Controller
// ============================================

class GreenhouseApp {
    constructor() {
        this.ui = new UIController();
        this.pollInterval = null;
    }

    async init() {
    // App init log removed

        // Subscribe to state changes
        appState.subscribe(state => this.onStateChange(state));

        // Check if authenticated
        if (!!appState.getState().apiToken) {
            this.ui.updateControlView();
        }

        // Switch to saved view
        this.ui.switchView(appState.getState().currentView);

        // Initial data fetch
        await this.updateSensors();
        await this.updateSystem();

        if (!!appState.getState().apiToken) {
            await this.updateRelays();
        }

        // Start polling
        this.startPolling();

    // App initialized log removed

        // Reflect auth UI on load and fetch relays if we already have a token
        this.ui.updateControlView();
        if (!!appState.getState().apiToken) {
            try { await this.updateRelays(); } catch (e) { /* ignore */ }
        }
    }

    onStateChange(state) {
        // React to state changes
        this.ui.updateConnectionStatus(state.isConnected);
        // Only toggle control view when token presence changes (auth changes)
        const authed = !!state.apiToken;
        if (this._lastAuth !== authed) {
            this.ui.updateControlView();
            this._lastAuth = authed;
        }
    }

    async updateSensors() {
        try {
            const data = await api.getSensors();
            const state = appState.getState();
            const history = [...state.sensors.history, data].slice(-CONFIG.CHART_MAX_POINTS);

            appState.setState({
                sensors: { ...data, history },
                isConnected: true,
                lastUpdate: new Date()
            });

            this.ui.updateSensorDisplay(data);
            this.ui.drawEnvironmentChart(history);
        } catch (error) {
            console.error('Failed to update sensors:', error);
            appState.setState({ isConnected: false });
        }
    }

    async updateSystem() {
        try {
            const data = await api.getSystemHealth();
            appState.setState({ system: data });
            this.ui.updateSystemDisplay(data);
        } catch (error) {
            console.error('Failed to update system:', error);
        }
    }

    async updateRelays() {
    if (!appState.getState().apiToken) return;

        try {
            const data = await api.getRelays();
            appState.setState({ relays: data });
            this.ui.updateRelaysDisplay(data);
        } catch (error) {
            console.warn('Update relays failed (non-fatal):', error.message || error);
            // Do not change auth state or hide UI on transient failures.
        }
    }

    async updateLogs() {
    if (!appState.getState().apiToken) return;

        try {
            const data = await api.getLogs();
            appState.setState({ logs: data });
        } catch (error) {
            console.error('Failed to update logs:', error);
        }
    }

    startPolling() {
        this.pollInterval = setInterval(async () => {
            await this.updateSensors();
            await this.updateSystem();

            if (appState.getState().apiToken) {
                await this.updateRelays();
            }
        }, CONFIG.POLL_INTERVAL);
    }

    stopPolling() {
        if (this.pollInterval) {
            clearInterval(this.pollInterval);
            this.pollInterval = null;
        }
    }
}

// ============================================
// Global Functions (for inline HTML calls)
// ============================================

async function toggleRelay(index, state) {
    try {
        await api.setRelay(index, state);
        await app.updateRelays();
    } catch (error) {
        app.ui.showAlert('Error al cambiar el estado del relé', 'danger');
    }
}

async function setRelayMode(index, mode) {
    try {
        await api.setRelayMode(index, mode);
        await app.updateRelays();
    } catch (error) {
        app.ui.showAlert('Error al cambiar el modo del relé', 'danger');
    }
}

// ============================================
// Initialize App
// ============================================

const app = new GreenhouseApp();

document.addEventListener('DOMContentLoaded', () => {
    app.init();
});

// Cleanup on unload
window.addEventListener('beforeunload', () => {
    app.stopPolling();
});
