/**
 * Relay & Rules Management
 * Handles individual relay control and rule editor
 */

class RelayManager {
    constructor() {
        this.relays = [
            { id: 0, name: 'luces', label: 'Luces', icon: 'i-light' },
            { id: 1, name: 'ventilador', label: 'Ventilador', icon: 'i-fan' },
            { id: 2, name: 'bomba', label: 'Bomba de Riego', icon: 'i-pump' },
            { id: 3, name: 'calefactor', label: 'Calefactor', icon: 'i-heater' }
        ];
        this.currentRelay = null;
        this.currentRuleIndex = null;
        this.setupEventListeners();
    }

    setupEventListeners() {
        // Toggle switches for each relay
        this.relays.forEach(relay => {
            const toggle = document.getElementById(`${relay.name}Toggle`);
            const modeSelect = document.getElementById(`${relay.name}Mode`);
            const refreshBtn = document.getElementById(`refresh${relay.label.replace(/ /g, '')}`);
            const addRuleBtn = document.getElementById(`${relay.name}AddRule`);

            if (toggle) {
                toggle.addEventListener('change', () => this.handleToggle(relay.id, toggle.checked));
            }

            if (modeSelect) {
                modeSelect.addEventListener('change', () => this.handleModeChange(relay.id, modeSelect.value));
            }

            if (refreshBtn) {
                refreshBtn.addEventListener('click', () => this.refreshRelay(relay.id));
            }

            if (addRuleBtn) {
                addRuleBtn.addEventListener('click', () => this.openRuleEditor(relay.id));
            }
        });

        // Rule modal events
        document.getElementById('ruleModalClose')?.addEventListener('click', () => this.closeRuleModal());
        document.getElementById('cancelRuleBtn')?.addEventListener('click', () => this.closeRuleModal());
        document.getElementById('ruleForm')?.addEventListener('submit', (e) => this.saveRule(e));

        // Condition modal events
        document.getElementById('addConditionBtn')?.addEventListener('click', () => this.openConditionModal());
        document.getElementById('conditionModalClose')?.addEventListener('click', () => this.closeConditionModal());
        document.getElementById('cancelConditionBtn')?.addEventListener('click', () => this.closeConditionModal());
        document.getElementById('conditionForm')?.addEventListener('submit', (e) => this.addCondition(e));

        // Condition type selector
        document.getElementById('conditionType')?.addEventListener('change', (e) => this.updateConditionFields(e.target.value));
        document.getElementById('sensorOp')?.addEventListener('change', (e) => {
            document.getElementById('sensorValueMax').style.display = e.target.value === 'between' ? 'block' : 'none';
        });
        document.getElementById('relayStateOp')?.addEventListener('change', (e) => {
            const show = e.target.value === 'on_duration' || e.target.value === 'off_duration';
            document.getElementById('relayDurationField').style.display = show ? 'block' : 'none';
        });
    }

    async handleToggle(relayId, state) {
        try {
            await api.setRelay(relayId, state ? 1 : 0);
            await this.refreshRelay(relayId);
        } catch (error) {
            console.error('Failed to toggle relay:', error);
            app.ui.showAlert('Error al cambiar relay', 'danger');
        }
    }

    async handleModeChange(relayId, mode) {
        try {
            await api.setRelayMode(relayId, mode);
            await this.refreshRelay(relayId);
            // Show/hide rules section
            const relay = this.relays.find(r => r.id === relayId);
            if (relay) {
                const rulesSection = document.getElementById(`${relay.name}Rules`);
                if (rulesSection) {
                    rulesSection.style.display = mode === 'auto' ? 'block' : 'none';
                }
            }
        } catch (error) {
            console.error('Failed to change mode:', error);
            app.ui.showAlert('Error al cambiar modo', 'danger');
        }
    }

    async refreshRelay(relayId) {
        try {
            const data = await api.getRelays();
            const relay = this.relays.find(r => r.id === relayId);
            if (!relay || !data.relays) return;

            const relayData = data.relays[relayId];
            if (!relayData) return;

            // Update UI
            const indicator = document.getElementById(`${relay.name}Indicator`);
            const status = document.getElementById(`${relay.name}Status`);
            const toggle = document.getElementById(`${relay.name}Toggle`);
            const modeSelect = document.getElementById(`${relay.name}Mode`);

            if (indicator) indicator.textContent = relayData.is_on ? '🟢' : '⚫';
            if (status) status.textContent = relayData.is_on ? 'Encendido' : 'Apagado';
            if (toggle) toggle.checked = relayData.is_on;
            if (modeSelect) modeSelect.value = relayData.mode;

            // Show/hide rules section
            const rulesSection = document.getElementById(`${relay.name}Rules`);
            if (rulesSection) {
                rulesSection.style.display = relayData.mode === 'auto' ? 'block' : 'none';
            }

            // Load rules (placeholder for now)
            this.loadRules(relayId);
        } catch (error) {
            console.error('Failed to refresh relay:', error);
        }
    }

    async loadRules(relayId) {
        const relay = this.relays.find(r => r.id === relayId);
        if (!relay) return;

        const rulesList = document.getElementById(`${relay.name}RulesList`);
        if (!rulesList) return;

        try {
            // Show loading state
            rulesList.innerHTML = '<p class="text-secondary">Cargando reglas...</p>';

            const response = await api.getRelayRules(relayId);
            const rules = response.rules || [];

            if (rules.length === 0) {
                rulesList.innerHTML = '<p class="text-secondary">Sin reglas configuradas</p>';
            } else {
                rulesList.innerHTML = rules.map((rule, idx) => this.renderRule(relayId, rule, idx)).join('');
            }
        } catch (error) {
            console.error('Failed to load rules:', error);
            rulesList.innerHTML = '<p class="text-danger">Error al cargar reglas</p>';
        }
    }

    renderRule(relayId, rule, index) {
        return `
            <div class="rule-item">
                <div class="rule-header">
                    <div class="rule-name">
                        ${rule.enabled ? '<span class="rule-enabled">✓</span>' : '<span class="rule-disabled">✗</span>'}
                        ${rule.name || `Regla ${index + 1}`}
                        <span class="badge badge-secondary" style="margin-left: 8px;">Prioridad: ${rule.priority || 0}</span>
                    </div>
                    <div class="rule-actions">
                        <button class="btn btn-sm btn-secondary" onclick="relayManager.editRule(${relayId}, ${index})">Editar</button>
                        <button class="btn btn-sm btn-danger" onclick="relayManager.deleteRule(${relayId}, ${index})">Eliminar</button>
                    </div>
                </div>
                <div class="rule-description">
                    <strong>Cuando:</strong> ${this.formatRuleConditions(rule.conditions)}
                </div>
                <div class="rule-summary">
                    <strong>Acción:</strong> ${this.formatRuleAction(rule.action)}
                    ${rule.cooldown_min > 0 ? ` (espera ${rule.cooldown_min} min entre activaciones)` : ''}
                </div>
            </div>
        `;
    }

    formatRuleConditions(conditions) {
        if (!conditions || conditions.length === 0) return 'Sin condiciones';
        
        return conditions.map(c => {
            switch (c.type) {
                case 'time_range':
                    return `⏰ Hora entre ${c.start_hour}:${String(c.start_min).padStart(2, '0')} - ${c.end_hour}:${String(c.end_min).padStart(2, '0')}`;
                case 'weekday':
                    const dayNames = ['Dom', 'Lun', 'Mar', 'Mié', 'Jue', 'Vie', 'Sáb'];
                    const selectedDays = c.days.map(d => dayNames[d]).join(', ');
                    return `📅 Días: ${selectedDays}`;
                case 'sensor':
                    const sensorName = c.sensor === 'temp' ? '🌡️ Temperatura' :
                                      c.sensor === 'humidity' ? '💧 Humedad' :
                                      c.sensor === 'soil' ? '🌱 Suelo' : c.sensor;
                    const opText = c.op === 'gt' ? '>' :
                                  c.op === 'lt' ? '<' :
                                  c.op === 'eq' ? '=' :
                                  c.op === 'between' ? 'entre' : c.op;
                    if (c.op === 'between') {
                        return `${sensorName} ${opText} ${c.value} y ${c.value2}`;
                    }
                    return `${sensorName} ${opText} ${c.value}`;
                case 'relay_state':
                    const relay = this.relays.find(r => r.id === c.relay_index);
                    const relayLabel = relay ? relay.label : `Relay ${c.relay_index}`;
                    if (c.state_op === 'is_on') return `🔌 ${relayLabel} encendido`;
                    if (c.state_op === 'is_off') return `🔌 ${relayLabel} apagado`;
                    if (c.state_op === 'on_duration') return `🔌 ${relayLabel} encendido hace ${c.duration_min} min`;
                    if (c.state_op === 'off_duration') return `🔌 ${relayLabel} apagado hace ${c.duration_min} min`;
                    return `🔌 ${relayLabel}`;
                default:
                    return '';
            }
        }).filter(t => t).join(' <strong>Y</strong> ');
    }

    formatRuleAction(action) {
        if (!action) return 'Sin acción';
        
        if (action.type === 'turn_on') {
            return action.duration_min > 0 
                ? `✅ Encender por ${action.duration_min} minutos`
                : '✅ Encender';
        }
        return '⛔ Apagar';
    }

    openRuleEditor(relayId, ruleIndex = null) {
        this.currentRelay = relayId;
        this.currentRuleIndex = ruleIndex;

        const modal = document.getElementById('ruleModal');
        const title = document.getElementById('ruleModalTitle');
        const relay = this.relays.find(r => r.id === relayId);

        if (title && relay) {
            title.textContent = ruleIndex !== null 
                ? `Editar Regla - ${relay.label}`
                : `Nueva Regla - ${relay.label}`;
        }

        // Clear form
        document.getElementById('ruleName').value = '';
        document.getElementById('ruleDuration').value = '';
        document.getElementById('rulePriority').value = '5';
        document.getElementById('ruleCooldown').value = '0';
        document.getElementById('conditionsList').innerHTML = '';

        modal.style.display = 'flex';
    }

    closeRuleModal() {
        document.getElementById('ruleModal').style.display = 'none';
        this.currentRelay = null;
        this.currentRuleIndex = null;
    }

    openConditionModal() {
        document.getElementById('conditionModal').style.display = 'flex';
        document.getElementById('conditionType').value = 'time_range';
        this.updateConditionFields('time_range');
    }

    closeConditionModal() {
        document.getElementById('conditionModal').style.display = 'none';
        document.getElementById('conditionForm').reset();
    }

    updateConditionFields(type) {
        // Hide all field sets
        document.querySelectorAll('.condition-fields').forEach(el => el.style.display = 'none');

        // Show relevant fields
        switch (type) {
            case 'time_range':
                document.getElementById('timeRangeFields').style.display = 'block';
                break;
            case 'weekday':
                document.getElementById('weekdayFields').style.display = 'block';
                break;
            case 'sensor':
                document.getElementById('sensorFields').style.display = 'block';
                break;
            case 'relay_state':
                document.getElementById('relayStateFields').style.display = 'block';
                break;
        }
    }

    addCondition(e) {
        e.preventDefault();
        const type = document.getElementById('conditionType').value;
        let condition = { type };
        let text = '';

        switch (type) {
            case 'time_range':
                const startTime = document.getElementById('timeStart').value;
                const endTime = document.getElementById('timeEnd').value;
                const [startHour, startMin] = startTime.split(':').map(Number);
                const [endHour, endMin] = endTime.split(':').map(Number);
                condition.start_hour = startHour;
                condition.start_min = startMin;
                condition.end_hour = endHour;
                condition.end_min = endMin;
                text = `Hora entre ${startTime} - ${endTime}`;
                break;
            case 'weekday':
                const days = Array.from(document.querySelectorAll('#weekdayFields input:checked'))
                    .map(cb => parseInt(cb.value));
                condition.days = days;
                const dayNames = ['D', 'L', 'M', 'X', 'J', 'V', 'S'];
                text = `Días: ${days.map(d => dayNames[d]).join(', ')}`;
                break;
            case 'sensor':
                condition.sensor = document.getElementById('sensorType').value;
                condition.op = document.getElementById('sensorOp').value;
                condition.value = parseFloat(document.getElementById('sensorValue').value);
                if (condition.op === 'between') {
                    condition.value2 = parseFloat(document.getElementById('sensorValue2').value);
                    text = `${condition.sensor} entre ${condition.value} y ${condition.value2}`;
                } else {
                    text = `${condition.sensor} ${condition.op} ${condition.value}`;
                }
                break;
            case 'relay_state':
                condition.relay_index = parseInt(document.getElementById('relayIndex').value);
                condition.state_op = document.getElementById('relayStateOp').value;
                const relayName = this.relays.find(r => r.id === condition.relay_index)?.label || 'Relay';
                if (condition.state_op === 'on_duration' || condition.state_op === 'off_duration') {
                    condition.duration_min = parseInt(document.getElementById('relayDuration').value);
                    text = `${relayName} ${condition.state_op === 'on_duration' ? 'encendido' : 'apagado'} hace ${condition.duration_min} min`;
                } else {
                    text = `${relayName} ${condition.state_op === 'on' ? 'encendido' : 'apagado'}`;
                }
                break;
        }

        // Add to list
        const conditionsList = document.getElementById('conditionsList');
        const conditionEl = document.createElement('div');
        conditionEl.className = 'condition-item';
        conditionEl.innerHTML = `
            <span class="condition-text">${text}</span>
            <button type="button" class="condition-remove" onclick="this.parentElement.remove()">×</button>
        `;
        conditionEl.dataset.condition = JSON.stringify(condition);
        conditionsList.appendChild(conditionEl);

        this.closeConditionModal();
    }

    async saveRule(e) {
        e.preventDefault();

        const name = document.getElementById('ruleName').value;
        const action = document.getElementById('ruleAction').value;
        const duration = parseInt(document.getElementById('ruleDuration').value) || 0;
        const priority = parseInt(document.getElementById('rulePriority').value);
        const cooldown = parseInt(document.getElementById('ruleCooldown').value);

        const conditions = Array.from(document.querySelectorAll('#conditionsList .condition-item'))
            .map(el => JSON.parse(el.dataset.condition));

        if (conditions.length === 0) {
            app.ui.showAlert('Debe agregar al menos una condición', 'warning');
            return;
        }

        const rule = {
            name,
            priority,
            enabled: true,
            conditions,
            action: {
                type: action,
                duration_min: duration
            },
            cooldown_min: cooldown
        };

        try {
            // Disable save button during operation
            const saveBtn = e.target.querySelector('button[type="submit"]');
            if (saveBtn) {
                saveBtn.disabled = true;
                saveBtn.textContent = 'Guardando...';
            }

            if (this.currentRuleIndex !== null) {
                // Update existing rule
                await api.updateRelayRule(this.currentRelay, this.currentRuleIndex, rule);
                app.ui.showAlert('Regla actualizada correctamente', 'success');
            } else {
                // Create new rule
                await api.saveRelayRule(this.currentRelay, rule);
                app.ui.showAlert('Regla creada correctamente', 'success');
            }

            this.closeRuleModal();
            this.loadRules(this.currentRelay);
        } catch (error) {
            console.error('Failed to save rule:', error);
            app.ui.showAlert('Error al guardar la regla', 'danger');
        }
    }

    async editRule(relayId, ruleIndex) {
        try {
            // Fetch current rules
            const response = await api.getRelayRules(relayId);
            const rules = response.rules || [];
            const rule = rules[ruleIndex];

            if (!rule) {
                app.ui.showAlert('No se encontró la regla', 'danger');
                return;
            }

            // Open modal with rule data
            this.currentRelay = relayId;
            this.currentRuleIndex = ruleIndex;

            const modal = document.getElementById('ruleModal');
            const title = document.getElementById('ruleModalTitle');
            const relay = this.relays.find(r => r.id === relayId);

            if (title && relay) {
                title.textContent = `Editar Regla - ${relay.label}`;
            }

            // Fill form with rule data
            document.getElementById('ruleName').value = rule.name || '';
            document.getElementById('ruleAction').value = rule.action?.type || 'turn_on';
            document.getElementById('ruleDuration').value = rule.action?.duration_min || 0;
            document.getElementById('rulePriority').value = rule.priority || 5;
            document.getElementById('ruleCooldown').value = rule.cooldown_min || 0;

            // Populate conditions
            const conditionsList = document.getElementById('conditionsList');
            conditionsList.innerHTML = '';
            
            if (rule.conditions && rule.conditions.length > 0) {
                rule.conditions.forEach(condition => {
                    const conditionEl = document.createElement('div');
                    conditionEl.className = 'condition-item';
                    conditionEl.innerHTML = `
                        <span class="condition-text">${this.formatConditionForDisplay(condition)}</span>
                        <button type="button" class="condition-remove" onclick="this.parentElement.remove()">×</button>
                    `;
                    conditionEl.dataset.condition = JSON.stringify(condition);
                    conditionsList.appendChild(conditionEl);
                });
            }

            modal.style.display = 'flex';
        } catch (error) {
            console.error('Failed to load rule for editing:', error);
            app.ui.showAlert('Error al cargar la regla', 'danger');
        }
    }

    formatConditionForDisplay(c) {
        switch (c.type) {
            case 'time_range':
                return `Hora entre ${c.start_hour}:${String(c.start_min).padStart(2, '0')} - ${c.end_hour}:${String(c.end_min).padStart(2, '0')}`;
            case 'weekday':
                const dayNames = ['Dom', 'Lun', 'Mar', 'Mié', 'Jue', 'Vie', 'Sáb'];
                return `Días: ${c.days.map(d => dayNames[d]).join(', ')}`;
            case 'sensor':
                const sensorName = c.sensor === 'temp' ? 'Temperatura' :
                                  c.sensor === 'humidity' ? 'Humedad' :
                                  c.sensor === 'soil' ? 'Suelo' : c.sensor;
                const opText = c.op === 'gt' ? '>' :
                              c.op === 'lt' ? '<' :
                              c.op === 'eq' ? '=' :
                              c.op === 'between' ? 'entre' : c.op;
                if (c.op === 'between') {
                    return `${sensorName} ${opText} ${c.value} y ${c.value2}`;
                }
                return `${sensorName} ${opText} ${c.value}`;
            case 'relay_state':
                const relay = this.relays.find(r => r.id === c.relay_index);
                const relayLabel = relay ? relay.label : `Relay ${c.relay_index}`;
                if (c.state_op === 'is_on') return `${relayLabel} encendido`;
                if (c.state_op === 'is_off') return `${relayLabel} apagado`;
                if (c.state_op === 'on_duration') return `${relayLabel} encendido hace ${c.duration_min} min`;
                if (c.state_op === 'off_duration') return `${relayLabel} apagado hace ${c.duration_min} min`;
                return relayLabel;
            default:
                return '';
        }
    }

    async deleteRule(relayId, ruleIndex) {
        if (!confirm('¿Eliminar esta regla? Esta acción no se puede deshacer.')) return;

        try {
            await api.deleteRelayRule(relayId, ruleIndex);
            app.ui.showAlert('Regla eliminada correctamente', 'success');
            this.loadRules(relayId);
        } catch (error) {
            console.error('Failed to delete rule:', error);
            app.ui.showAlert('Error al eliminar la regla', 'danger');
        }
    }

    async clearAllRules(relayId) {
        if (!confirm('¿Eliminar TODAS las reglas de este relay? Esta acción no se puede deshacer.')) return;

        try {
            await api.clearRelayRules(relayId);
            app.ui.showAlert('Todas las reglas han sido eliminadas', 'success');
            this.loadRules(relayId);
        } catch (error) {
            console.error('Failed to clear rules:', error);
            app.ui.showAlert('Error al eliminar las reglas', 'danger');
        }
    }
}

// Initialize global relay manager
const relayManager = new RelayManager();
