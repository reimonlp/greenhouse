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
    WS_URL: `ws://${window.location.hostname}/ws`,
    POLL_INTERVAL: 8000,
    CHART_MAX_POINTS: 50,
    STORAGE_KEY_TOKEN: 'gh_api_token',
    STORAGE_KEY_VIEW: 'gh_active_view',
    WS_RECONNECT_DELAY: 3000,
    WS_PING_INTERVAL: 30000
};

// ============================================
// WebSocket Manager
// ============================================

class WebSocketManager {
    constructor() {
        this.ws = null;
        this.isConnected = false;
        this.reconnectTimer = null;
        this.pingTimer = null;
        this.messageHandlers = {};
        this.requestId = 0;
        this.pendingRequests = new Map();
    }

    connect(token) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            return; // Already connected
        }

        console.log('[WS] Connecting to', CONFIG.WS_URL);
        this.ws = new WebSocket(CONFIG.WS_URL);
        
        this.ws.onopen = () => {
            console.log('[WS] Connected');
            this.isConnected = true;
            this.startPingTimer();
            this.emit('connected');
        };

        this.ws.onclose = () => {
            console.log('[WS] Disconnected');
            this.isConnected = false;
            this.stopPingTimer();
            this.emit('disconnected');
            this.scheduleReconnect(token);
        };

        this.ws.onerror = (error) => {
            console.error('[WS] Error:', error);
            this.emit('error', error);
        };

        this.ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                this.handleMessage(data);
            } catch (error) {
                console.error('[WS] Failed to parse message:', error);
            }
        };
    }

    disconnect() {
        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }
        this.stopPingTimer();
        if (this.ws) {
            this.ws.close();
            this.ws = null;
        }
        this.isConnected = false;
    }

    scheduleReconnect(token) {
        if (this.reconnectTimer) return;
        this.reconnectTimer = setTimeout(() => {
            this.reconnectTimer = null;
            this.connect(token);
        }, CONFIG.WS_RECONNECT_DELAY);
    }

    startPingTimer() {
        this.stopPingTimer();
        this.pingTimer = setInterval(() => {
            this.send({ type: 'ping' });
        }, CONFIG.WS_PING_INTERVAL);
    }

    stopPingTimer() {
        if (this.pingTimer) {
            clearInterval(this.pingTimer);
            this.pingTimer = null;
        }
    }

    send(data) {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
            console.warn('[WS] Cannot send, not connected');
            return false;
        }
        this.ws.send(JSON.stringify(data));
        return true;
    }

    sendRequest(data) {
        return new Promise((resolve, reject) => {
            const id = ++this.requestId;
            data.id = id;
            
            if (this.send(data)) {
                this.pendingRequests.set(id, { resolve, reject });
                
                // Timeout after 10 seconds
                setTimeout(() => {
                    if (this.pendingRequests.has(id)) {
                        this.pendingRequests.delete(id);
                        reject(new Error('Request timeout'));
                    }
                }, 10000);
            } else {
                reject(new Error('WebSocket not connected'));
            }
        });
    }

    handleMessage(data) {
        // Handle request responses
        if (data.requestId && this.pendingRequests.has(data.requestId)) {
            const { resolve } = this.pendingRequests.get(data.requestId);
            this.pendingRequests.delete(data.requestId);
            resolve(data);
            return;
        }

        // Emit type-specific events
        if (data.type) {
            this.emit(data.type, data);
        }
    }

    on(event, handler) {
        if (!this.messageHandlers[event]) {
            this.messageHandlers[event] = [];
        }
        this.messageHandlers[event].push(handler);
    }

    off(event, handler) {
        if (this.messageHandlers[event]) {
            this.messageHandlers[event] = this.messageHandlers[event].filter(h => h !== handler);
        }
    }

    emit(event, data) {
        if (this.messageHandlers[event]) {
            this.messageHandlers[event].forEach(handler => handler(data));
        }
    }
}

// Global WebSocket instance
const wsManager = new WebSocketManager();

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
            currentView: this.loadView() || 'dht',
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
        // Try WebSocket first, fallback to HTTP
        if (wsManager.isConnected) {
            try {
                return await wsManager.sendRequest({
                    type: 'setRelay',
                    relay: index,
                    state: state,
                    token: appState.getState().apiToken
                });
            } catch (error) {
                console.warn('[WS] setRelay failed, falling back to HTTP:', error);
            }
        }
        
        return this.fetch('/api/relays/set', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: `relay=${index}&state=${state}`
        });
    }

    async setRelayMode(index, mode) {
        // Try WebSocket first, fallback to HTTP
        if (wsManager.isConnected) {
            try {
                return await wsManager.sendRequest({
                    type: 'setRelayMode',
                    relay: index,
                    mode: mode,
                    token: appState.getState().apiToken
                });
            } catch (error) {
                console.warn('[WS] setRelayMode failed, falling back to HTTP:', error);
            }
        }
        
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

    // Rule management endpoints
    async getRelayRules(relayId) {
        return this.fetch(`/api/relays/${relayId}/rules`);
    }

    async saveRelayRule(relayId, rule) {
        return this.fetch(`/api/relays/${relayId}/rules`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(rule)
        });
    }

    async updateRelayRule(relayId, ruleIndex, rule) {
        return this.fetch(`/api/relays/${relayId}/rules/${ruleIndex}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(rule)
        });
    }

    async deleteRelayRule(relayId, ruleIndex) {
        return this.fetch(`/api/relays/${relayId}/rules/${ruleIndex}`, {
            method: 'DELETE'
        });
    }

    async clearRelayRules(relayId) {
        return this.fetch(`/api/relays/${relayId}/rules`, {
            method: 'DELETE'
        });
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
            topTabs: document.querySelectorAll('.top-tab'),
            views: document.querySelectorAll('.view'),
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

            // (Dashboard removed) quick summary bindings no longer used
            // DHT/NTC dedicated views
            temperature_dht: document.getElementById('temperature_dht'),
            humidity_dht: document.getElementById('humidity_dht'),
            dht_status_dht: document.getElementById('dht_status_dht'),
            // NTC removed
            // Soil dedicated view
            soil1_soil: document.getElementById('soil1_soil'),
            soil2_soil: document.getElementById('soil2_soil'),
            soil_status_soil: document.getElementById('soil_status_soil'),
            
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

            // Charts (NTC removed)
            envChart: document.getElementById('envChart'),

            // Logs
            logsList: document.getElementById('logsList'),
            logsFilter: document.getElementById('logsFilter'),

            // Relay views (new - separate tabs)
            lucesView: document.getElementById('lucesView'),
            ventiladorView: document.getElementById('ventiladorView'),
            bombaView: document.getElementById('bombaView'),
            calefactorView: document.getElementById('calefactorView')
        };
    }

    setupEventListeners() {
        // Sidebar toggle
        this.elements.sidebarToggle?.addEventListener('click', () => this.toggleSidebar());

        // Navigation
        this.elements.navItems.forEach(item => {
            item.addEventListener('click', () => this.switchView(item.dataset.view));
        });

        // Top tab navigation (mobile)
        this.elements.topTabs.forEach(tab => {
            tab.addEventListener('click', () => {
                this.elements.topTabs.forEach(t => t.classList.toggle('active', t === tab));
                this.switchView(tab.dataset.view);
            });
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

        // Help tooltips: click to show, click outside to hide
        document.querySelectorAll('.help-icon').forEach(el => {
            el.addEventListener('click', (e) => {
                const msg = el.getAttribute('data-help') || '';
                this.showTooltip(e.clientX, e.clientY, msg);
                e.stopPropagation();
            });
        });
        document.addEventListener('click', () => this.hideTooltip());

        // Logs: toggle details with event delegation
        this.elements.logsList?.addEventListener('click', (e) => {
            const btn = e.target.closest('.log-toggle');
            if (!btn) return;
            const row = btn.closest('.log-row');
            const pre = row?.querySelector('.log-data');
            if (!pre) return;
            const visible = pre.style.display === 'block';
            pre.style.display = visible ? 'none' : 'block';
            btn.textContent = visible ? 'Detalles' : 'Ocultar';
        });

        // Logs: filter change triggers rerender
        this.elements.logsFilter?.addEventListener('change', () => {
            const state = appState.getState();
            this.renderLogs(state.logs);
        });
    }

    toggleSidebar() {
        this.elements.sidebar?.classList.toggle('collapsed');
    }

    switchView(viewName) {
        // Update navigation state
        this.elements.navItems.forEach(item => {
            item.classList.toggle('active', item.dataset.view === viewName);
        });

        // Toggle tab views (no scrolling). If viewName doesn't exist, fall back to 'dht'
        const hasView = Array.from(this.elements.views).some(v => v.dataset.view === viewName);
        const targetView = hasView ? viewName : 'dht';
        this.elements.views.forEach(v => {
            v.classList.toggle('active', v.dataset.view === targetView);
        });
        // Sync top tabs active state
        this.elements.topTabs.forEach(t => t.classList.toggle('active', t.dataset.view === targetView));

        // If navigating to logs, refresh once
        if (targetView === 'logs') {
            if (!appState.getState().apiToken) {
                // Inform about auth requirement
                if (this.elements.logsList) {
                    this.elements.logsList.innerHTML = '<div style="color:var(--text-secondary)">Autenticación requerida para ver logs. Pulsa la llave para ingresar el token.</div>';
                }
            } else {
                this.refreshLogs();
            }
        }
        // Save preference
        appState.saveView(targetView);
        appState.setState({ currentView: targetView });
    }

    showTooltip(x, y, message) {
        const tip = document.getElementById('tooltip');
        if (!tip) return;
        tip.textContent = message;
        tip.style.display = 'block';
        // Position with small offset and bounds clamp
        const offset = 10;
        let left = x + offset;
        let top = y + offset;
        const vw = window.innerWidth;
        const vh = window.innerHeight;
        tip.style.left = left + 'px';
        tip.style.top = top + 'px';
        // Clamp if overflow
        const rect = tip.getBoundingClientRect();
        if (rect.right > vw) tip.style.left = (vw - rect.width - offset) + 'px';
        if (rect.bottom > vh) tip.style.top = (vh - rect.height - offset) + 'px';
    }

    hideTooltip() {
        const tip = document.getElementById('tooltip');
        if (tip) tip.style.display = 'none';
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
        // DHT view mirrors env values
        if (this.elements.temperature_dht) {
            this.elements.temperature_dht.textContent = data.temperature ? `${data.temperature.toFixed(1)}°C` : '--°C';
        }
        if (this.elements.humidity_dht) {
            this.elements.humidity_dht.textContent = data.humidity ? `${data.humidity.toFixed(1)}%` : '--%';
        }
        
        // Soil moisture
        if (this.elements.soil1_soil) {
            this.elements.soil1_soil.textContent = data.soil_moisture_1 !== undefined ? `${data.soil_moisture_1.toFixed(1)}%` : '--%';
        }
        if (this.elements.soil2_soil) {
            this.elements.soil2_soil.textContent = data.soil_moisture_2 !== undefined ? `${data.soil_moisture_2.toFixed(1)}%` : '--%';
        }
        
        // External temperature sensors
        // NTC removed
        
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
    // External temps removed

        if (this.elements.dht_status_dht) {
            this.elements.dht_status_dht.textContent = dhtValid ? '🟢' : '🔴';
        }
        if (this.elements.soil_status_soil) {
            this.elements.soil_status_soil.textContent = soilValid ? '🟢' : '🔴';
        }
        // NTC status removed
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
        
        // Update old control grid view if it exists (for backward compatibility)
        if (grid && data && data.relays) {
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
            grid.style.display = 'grid';
        }

        // Update individual relay views (new UI)
        if (data && data.relays && typeof relayManager !== 'undefined') {
            data.relays.forEach((relay, index) => {
                relayManager.refreshRelay(index);
            });
        }

        // Update quick stats
        if (data && data.relays) {
            const activeCount = data.relays.filter(r => r.state).length;
            const quick = document.getElementById('quickRelays');
            if (quick) quick.textContent = `${activeCount}/4`;
        }
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

    // drawNTCChart removed

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

    renderLogs(data) {
        const container = this.elements.logsList;
        if (!container) return;
        try {
            // Accept both array and {logs: []}
            // Also accept MongoDB Data API shape: {documents: [...]}
            let entries = Array.isArray(data)
                ? data
                : (Array.isArray(data.logs)
                    ? data.logs
                    : (Array.isArray(data.documents) ? data.documents : []));
            if (!entries.length) {
                container.innerHTML = '<div style="color:var(--text-secondary)">Sin registros.</div>';
                return;
            }
            // Newest first by timestamp
            entries.sort((a, b) => (b.timestamp || b.ts || 0) - (a.timestamp || a.ts || 0));

            // Filter by level if requested
            const filter = this.elements.logsFilter?.value || 'all';
            const lvlNum = (e) => (typeof e.level !== 'undefined') ? e.level : e.lvl;
            entries = entries.filter(e => {
                const l = lvlNum(e) ?? 1;
                if (filter === 'crit') return l >= 4;
                if (filter === 'error+') return l >= 3;
                if (filter === 'warn+') return l >= 2;
                return true;
            });
            if (!entries.length) {
                container.innerHTML = '<div style="color:var(--text-secondary)">Sin registros para este filtro.</div>';
                return;
            }

            const esc = (s) => String(s ?? '')
                .replace(/&/g, '&amp;')
                .replace(/</g, '&lt;')
                .replace(/>/g, '&gt;');

            const levelClass = (lvlNum) => {
                switch (lvlNum) {
                    case 0: return 'level-debug';
                    case 1: return 'level-info';
                    case 2: return 'level-warn';
                    case 3: return 'level-error';
                    case 4: return 'level-crit';
                    default: return 'level-info';
                }
            };

            const levelText = (lvlNum) => ({0:'DEBUG',1:'INFO',2:'WARN',3:'ERROR',4:'CRIT'})[lvlNum] || 'INFO';

            const colorizeJSON = (jsonStr) => {
                // Super simple syntax highlighter for JSON
                const s = esc(jsonStr);
                return s
                    .replace(/(&quot;.*?&quot;)(\s*:\s*)/g, '<span class="j-key">$1</span>$2')
                    .replace(/:\s*(&quot;.*?&quot;)/g, ': <span class="j-str">$1</span>')
                    .replace(/:\s*(\d+\.?\d*)/g, ': <span class="j-num">$1</span>')
                    .replace(/:\s*(true|false|null)/g, ': <span class="j-bool">$1</span>');
            };

            const html = entries.map(e => {
                const ts = e.timestamp ?? e.ts ?? 0;
                const date = ts ? new Date(ts * 1000).toLocaleString() : '';
                const lvlNum = (typeof e.level !== 'undefined') ? e.level : e.lvl;
                const lvlCls = levelClass(lvlNum);
                const lvlTxt = levelText(lvlNum);
                const src = e.source || e.src || '';
                const msg = e.message || e.msg || '';
                const extra = (typeof e.data !== 'undefined') ? e.data : (typeof e.data_raw !== 'undefined' ? e.data_raw : null);
                const hasExtra = extra !== null && extra !== '';
                const rawText = typeof extra === 'string' ? extra : JSON.stringify(extra, null, 2);
                const extraText = colorizeJSON(rawText);

                return `
                <div class="log-row">
                    <span class="log-ts">${esc(date)}</span>
                    <span class="log-level ${lvlCls}">${lvlTxt}</span>
                    <span class="log-src">${esc(src)}</span>
                    <span class="log-msg">${esc(msg)}</span>
                    ${hasExtra ? `<button class="log-toggle">Detalles</button>` : ''}
                    ${hasExtra ? `<pre class="log-data" style="display:none;"><code>${extraText}</code></pre>` : ''}
                </div>`;
            }).join('');
            container.innerHTML = html;
        } catch (err) {
            container.textContent = 'Error al renderizar logs';
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

        // Setup WebSocket handlers
        this.setupWebSocketHandlers();

        // Connect WebSocket if authenticated
        if (!!appState.getState().apiToken) {
            wsManager.connect(appState.getState().apiToken);
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

        // Start polling (fallback for when WebSocket disconnects)
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
            this.ui?.renderLogs?.(data);
        } catch (error) {
            console.error('Failed to update logs:', error);
        }
    }

    setupWebSocketHandlers() {
        // Handle real-time sensor updates
        wsManager.on('sensors', (data) => {
            const state = appState.getState();
            const history = [...state.sensors.history, data].slice(-CONFIG.CHART_MAX_POINTS);
            appState.setState({
                sensors: { ...data, history },
                lastUpdate: Date.now()
            });
            this.ui.updateSensorDisplay(data);
        });

        // Handle real-time relay state updates
        wsManager.on('relayState', (data) => {
            const state = appState.getState();
            if (state.relays && state.relays.relays) {
                // Update specific relay in state
                const relays = {...state.relays};
                relays.relays[data.relay] = {
                    ...relays.relays[data.relay],
                    is_on: data.is_on,
                    mode: data.mode
                };
                appState.setState({ relays });
                this.ui.updateRelaysDisplay(relays);
            }
            // Notify relay manager if active
            if (window.relayManager) {
                window.relayManager.refreshRelay(data.relay);
            }
        });

        // Handle relay mode updates
        wsManager.on('relayMode', (data) => {
            const state = appState.getState();
            if (state.relays && state.relays.relays) {
                const relays = {...state.relays};
                relays.relays[data.relay] = {
                    ...relays.relays[data.relay],
                    mode: data.mode
                };
                appState.setState({ relays });
                this.ui.updateRelaysDisplay(relays);
            }
            if (window.relayManager) {
                window.relayManager.refreshRelay(data.relay);
            }
        });

        // Handle rule events
        wsManager.on('ruleEvent', (data) => {
            console.log('[Rule Event]', data);
            this.ui.showAlert(`Regla activada: ${data.rule} → ${data.action}`, 'info');
        });

        // Handle system status updates
        wsManager.on('systemStatus', (data) => {
            appState.setState({ system: data });
            this.ui.updateSystemDisplay(data);
        });

        // Handle connection events
        wsManager.on('connected', () => {
            console.log('[WS] Connected');
            appState.setState({ isConnected: true });
            this.ui.showAlert('Conexión WebSocket establecida', 'success');
        });

        wsManager.on('disconnected', () => {
            console.log('[WS] Disconnected');
            appState.setState({ isConnected: false });
            this.ui.showAlert('Conexión WebSocket perdida, reintentando...', 'warning');
        });
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
