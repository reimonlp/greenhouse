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

export function useRelayUpdates() {
  const [relayStates, setRelayStates] = useState([]);

  // Solicitar estado inicial al montar
  useEffect(() => {
    if (webSocketService.socket && webSocketService.socket.connected) {
      webSocketService.socket.emit('relay:states');
    } else {
      // Si no está conectado aún, esperar a que conecte
      const onConnect = () => {
        webSocketService.socket.emit('relay:states');
      };
      webSocketService.socket?.on('connect', onConnect);
      return () => {
        webSocketService.socket?.off('connect', onConnect);
      };
    }
  }, []);

  // Actualizar con el array inicial
  useWebSocketEvent('relay:states', (response) => {
    console.log('[WS] Evento relay:states', response);
    if (response.success && Array.isArray(response.data)) {
      setRelayStates(response.data);
    } else {
      console.warn('[WS] relay:states error o datos vacíos', response);
    }
  });

  // Actualizar con cambios individuales
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

  useWebSocketEvent('relay:changed', (data) => {
    console.log('[WS] Evento relay:changed', data);
    handleRelayChange(data);
  });

  return relayStates;
}
