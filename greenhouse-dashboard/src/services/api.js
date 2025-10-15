import axios from 'axios';
import { API_ENDPOINTS } from '../config/api';

// Axios instance con configuración común
const api = axios.create({
  timeout: 10000,
  headers: {
    'Content-Type': 'application/json',
  },
});

// Interceptor para logging de errores
api.interceptors.response.use(
  (response) => response,
  (error) => {
    console.error('API Error:', error.response?.data || error.message);
    return Promise.reject(error);
  }
);

// ========== Health Check ==========
export const checkHealth = async () => {
  const response = await api.get(API_ENDPOINTS.HEALTH);
  return response.data;
};

// ========== Sensors ==========
export const getSensorReadings = async (limit = 100) => {
  const response = await api.get(`${API_ENDPOINTS.SENSORS}?limit=${limit}`);
  return response.data;
};

export const getLatestSensorReading = async () => {
  const response = await api.get(API_ENDPOINTS.SENSORS_LATEST);
  return response.data;
};

export const sendSensorData = async (data) => {
  const response = await api.post(API_ENDPOINTS.SENSORS, data);
  return response.data;
};

// ========== Relays ==========
export const getRelayStates = async () => {
  const response = await api.get(API_ENDPOINTS.RELAY_STATES);
  return response.data;
};

export const setRelayState = async (relayId, state, mode = 'manual', changedBy = 'user') => {
  const response = await api.post(API_ENDPOINTS.RELAY_STATE(relayId), {
    state,
    mode,
    changed_by: changedBy,
  });
  return response.data;
};

// ========== Rules ==========
export const getRules = async (relayId = null) => {
  const url = relayId !== null 
    ? `${API_ENDPOINTS.RULES}?relay_id=${relayId}`
    : API_ENDPOINTS.RULES;
  const response = await api.get(url);
  return response.data;
};

export const createRule = async (ruleData) => {
  const response = await api.post(API_ENDPOINTS.RULES, ruleData);
  return response.data;
};

export const updateRule = async (ruleId, ruleData) => {
  const response = await api.put(API_ENDPOINTS.RULE_BY_ID(ruleId), ruleData);
  return response.data;
};

export const deleteRule = async (ruleId) => {
  const response = await api.delete(API_ENDPOINTS.RULE_BY_ID(ruleId));
  return response.data;
};

// ========== Logs ==========
export const getLogs = async (limit = 50, level = null, source = null) => {
  let url = `${API_ENDPOINTS.LOGS}?limit=${limit}`;
  if (level) url += `&level=${level}`;
  if (source) url += `&source=${source}`;
  
  const response = await api.get(url);
  return response.data;
};

export default api;
