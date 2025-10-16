import { useEffect, useState, useCallback } from 'react';
import webSocketService from '../services/websocket';

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
    const unsubscribe = webSocketService.on(event, callback);
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

export function useRelayUpdates() {
  const [relayStates, setRelayStates] = useState([]);

  const handleRelayChange = useCallback((data) => {
    setRelayStates(prev => {
      const index = prev.findIndex(r => r.relay_id === data.relay_id);
      if (index >= 0) {
        const updated = [...prev];
        updated[index] = data;
        return updated;
      }
      return [...prev, data];
    });
  }, []);

  useWebSocketEvent('relay:changed', handleRelayChange);

  return relayStates;
}
