import { useEffect, useState, useCallback } from 'react';
import webSocketService from '../services/websocket';

export function useStormEvent() {
  const [stormData, setStormData] = useState(null);
  const handleStorm = useCallback((data) => {
    setStormData(data);
  }, []);
  useWebSocketEvent('sensor:storm', handleStorm);
  return stormData;
}

export function useWebSocket() {
  const [isConnected, setIsConnected] = useState(false);

  useEffect(() => {
    // Conectar al montar
    webSocketService.connect();

    // Escuchar cambios de estado de conexión
    const unsubscribe = webSocketService.on('connection:status', ({ connected }) => {
      setIsConnected(connected);
    });

    // Desconectar al desmontar
    return () => {
      unsubscribe();
      // No desconectamos el servicio aquí porque puede haber otros componentes usándolo
    };
  }, []);

  return { isConnected };
}

export function useWebSocketEvent(event, callback) {
  useEffect(() => {
    const wrappedCallback = (...args) => {
      console.log(`[WS] Evento recibido: ${event}`, ...args);
      callback(...args);
    };
    const unsubscribe = webSocketService.on(event, wrappedCallback);
    return unsubscribe;
  }, [event, callback]);
}

export function useSensorUpdates() {
  const [latestSensor, setLatestSensor] = useState(null);

  const handleSensorUpdate = useCallback((data) => {
    setLatestSensor(data);
  }, []);

  useWebSocketEvent('sensor:new', handleSensorUpdate);

  return latestSensor;
}

export function useSensorHistory(limit = 100, startDate = null, endDate = null) {
  const [sensorHistory, setSensorHistory] = useState([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);

  const fetchHistory = useCallback(() => {
    setLoading(true);
    webSocketService.fetchSensorHistory(limit, startDate, endDate);
  }, [limit, startDate, endDate]);

  useEffect(() => {
    const unsubscribe = webSocketService.on('sensor:history', (response) => {
      if (response.success) {
        setSensorHistory(response.data);
        setError(null);
      } else {
        setError(response.error);
      }
      setLoading(false);
    });

    return unsubscribe;
  }, []);

  return { sensorHistory, loading, error, fetchHistory };
}

export function useRelayUpdates() {
  const [relayStates, setRelayStates] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  // Solicitar estado inicial al conectar
  useEffect(() => {
    const requestRelayStates = () => {
      setLoading(true);
      webSocketService.fetchRelayStates();
    };

    // Escuchar el evento de conexión establecida
    const unsubscribe = webSocketService.on('connection:status', ({ connected }) => {
      if (connected) {
        // Pequeño delay para asegurar que el socket está listo
        setTimeout(() => {
          requestRelayStates();
        }, 100);
      }
    });

    // Si ya está conectado, solicitar ahora
    if (webSocketService.socket?.connected) {
      requestRelayStates();
    }

    return unsubscribe;
  }, []);

  // Listener para relay:states response
  useEffect(() => {
    const unsubscribe = webSocketService.on('relay:states', (response) => {
      console.log('[WS] Evento relay:states recibido:', response);
      if (response.success && Array.isArray(response.data)) {
        // Ordenar por relay_id para consistencia
        const sorted = response.data.sort((a, b) => a.relay_id - b.relay_id);
        setRelayStates(sorted);
        setError(null);
      } else {
        console.warn('[WS] relay:states error:', response.error);
        setError(response.error || 'Error al cargar estados de relés');
      }
      setLoading(false);
    });

    return unsubscribe;
  }, []);

  // Listener para cambios individuales de relés
  useEffect(() => {
    const unsubscribe = webSocketService.on('relay:changed', (data) => {
      console.log('[WS] Evento relay:changed recibido:', data);
      setRelayStates(prev => {
        const index = prev.findIndex(r => r.relay_id === data.relay_id);
        if (index >= 0) {
          const updated = [...prev];
          updated[index] = data;
          return updated;
        }
        // Si no existe, agregarlo
        return [...prev, data].sort((a, b) => a.relay_id - b.relay_id);
      });
    });

    return unsubscribe;
  }, []);

  return { relayStates, loading, error };
}

export function useRules() {
  const [rules, setRules] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  const fetchRules = useCallback(() => {
    setLoading(true);
    webSocketService.fetchRules();
  }, []);

  // Solicitar reglas al conectar
  useEffect(() => {
    const requestRules = () => {
      setLoading(true);
      webSocketService.fetchRules();
    };

    // Escuchar el evento de conexión establecida
    const unsubscribe = webSocketService.on('connection:status', ({ connected }) => {
      if (connected) {
        // Pequeño delay para asegurar que el socket está listo
        setTimeout(() => {
          requestRules();
        }, 100);
      }
    });

    // Si ya está conectado, solicitar ahora
    if (webSocketService.socket?.connected) {
      requestRules();
    }

    return unsubscribe;
  }, []);

  // Listener para rule:list response
  useEffect(() => {
    const unsubscribeList = webSocketService.on('rule:list', (response) => {
      if (response.success) {
        setRules(response.data);
        setError(null);
      } else {
        setError(response.error);
      }
      setLoading(false);
    });

    return unsubscribeList;
  }, []);

  // Listener para rule:created
  useEffect(() => {
    const unsubscribeCreated = webSocketService.on('rule:created', (response) => {
      if (response.success) {
        setRules(prev => [...prev, response.data]);
      }
    });

    return unsubscribeCreated;
  }, []);

  // Listener para rule:updated
  useEffect(() => {
    const unsubscribeUpdated = webSocketService.on('rule:updated', (response) => {
      if (response.success) {
        setRules(prev => prev.map(r => r._id === response.data._id ? response.data : r));
      }
    });

    return unsubscribeUpdated;
  }, []);

  // Listener para rule:deleted
  useEffect(() => {
    const unsubscribeDeleted = webSocketService.on('rule:deleted', (response) => {
      if (response.success) {
        setRules(prev => prev.filter(r => r._id !== response.data.id));
      }
    });

    return unsubscribeDeleted;
  }, []);

  return { rules, loading, error, fetchRules };
}

export function useLogs(limit = 50, level = null, source = null) {
  const [logs, setLogs] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  const fetchLogs = useCallback(() => {
    setLoading(true);
    webSocketService.fetchLogs(limit, level, source);
  }, [limit, level, source]);

  // Solicitar logs al conectar
  useEffect(() => {
    const requestLogs = () => {
      setLoading(true);
      webSocketService.fetchLogs(limit, level, source);
    };

    // Escuchar el evento de conexión establecida
    const unsubscribe = webSocketService.on('connection:status', ({ connected }) => {
      if (connected) {
        // Pequeño delay para asegurar que el socket está listo
        setTimeout(() => {
          requestLogs();
        }, 100);
      }
    });

    // Si ya está conectado, solicitar ahora
    if (webSocketService.socket?.connected) {
      requestLogs();
    }

    return unsubscribe;
  }, [limit, level, source]);

  // Listener para log:list response
  useEffect(() => {
    const unsubscribe = webSocketService.on('log:list', (response) => {
      if (response.success) {
        setLogs(response.data);
        setError(null);
      } else {
        setError(response.error);
      }
      setLoading(false);
    });

    return unsubscribe;
  }, []);

  return { logs, loading, error, fetchLogs };
}
