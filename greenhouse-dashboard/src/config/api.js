// API Configuration
const API_BASE_URL = import.meta.env.VITE_API_URL || 'http://168.181.185.42';

export const API_ENDPOINTS = {
  HEALTH: `${API_BASE_URL}/health`,
  SENSORS: `${API_BASE_URL}/api/sensors`,
  SENSORS_LATEST: `${API_BASE_URL}/api/sensors/latest`,
  RELAY_STATES: `${API_BASE_URL}/api/relays/states`,
  RELAY_STATE: (id) => `${API_BASE_URL}/api/relays/${id}/state`,
  RULES: `${API_BASE_URL}/api/rules`,
  RULE_BY_ID: (id) => `${API_BASE_URL}/api/rules/${id}`,
  LOGS: `${API_BASE_URL}/api/logs`,
};

export default API_BASE_URL;
