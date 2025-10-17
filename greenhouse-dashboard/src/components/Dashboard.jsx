import { useState, useEffect } from 'react';
import { 
  Box, 
  Container, 
  Grid, 
  Typography, 
  AppBar, 
  Toolbar,
  Paper,
  Alert,
  CircularProgress,
  Chip
} from '@mui/material';
import { Thermostat, Opacity, WaterDrop, WifiOff, Wifi } from '@mui/icons-material';
import SensorCard from './SensorCard';
import RelayControl from './RelayControl';
import RuleManager from './RuleManager';
import SensorChart from './SensorChart';
import LogViewer from './LogViewer';
import { getLatestSensorReading, getRelayStates } from '../services/api';
import { useWebSocket, useSensorUpdates } from '../hooks/useWebSocket';

function Dashboard() {
  const [sensorData, setSensorData] = useState(null);
  const [relayStates, setRelayStates] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  
  // WebSocket hooks
  const { isConnected } = useWebSocket();
  const latestSensor = useSensorUpdates();

  // Actualizar sensores cuando llegan por WebSocket
  useEffect(() => {
    if (latestSensor) {
      setSensorData(latestSensor);
    }
  }, [latestSensor]);

  // Escuchar cambios de relay por WebSocket
  useEffect(() => {
    const handleRelayChange = (data) => {
      setRelayStates(prev => {
        const index = prev.findIndex(r => r.relay_id === data.relay_id);
        if (index >= 0) {
          const updated = [...prev];
          updated[index] = data;
          return updated;
        }
        return prev; // No agregar si no existe
      });
    };

    // Importar el servicio y suscribirse
    import('../services/websocket').then(({ default: ws }) => {
      const unsubscribe = ws.on('relay:changed', handleRelayChange);
      return unsubscribe;
    });
  }, []);

  // Cargar datos iniciales
  const fetchData = async () => {
    try {
      setError(null);
      
      // Fetch sensor data
      const sensorResponse = await getLatestSensorReading();
      if (sensorResponse.success && sensorResponse.data) {
        setSensorData(sensorResponse.data);
      }

      // Fetch relay states
      const relayResponse = await getRelayStates();
      if (relayResponse.success && relayResponse.data) {
        setRelayStates(relayResponse.data);
      }

      setLoading(false);
    } catch (err) {
      console.error('Error fetching data:', err);
      setError('Error al cargar datos del servidor');
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchData();
    // Ya no necesitamos polling porque WebSocket actualiza en tiempo real
  }, []);

  const handleRelayToggle = async () => {
    // Refresh relay states after toggle
    try {
      const relayResponse = await getRelayStates();
      if (relayResponse.success && relayResponse.data) {
        setRelayStates(relayResponse.data);
      }
    } catch (err) {
      console.error('Error refreshing relay states:', err);
    }
  };

  if (loading) {
    return (
      <Box sx={{ display: 'flex', justifyContent: 'center', alignItems: 'center', minHeight: '100vh' }}>
        <CircularProgress size={60} />
      </Box>
    );
  }

  return (
    <Box sx={{ flexGrow: 1, bgcolor: '#f5f5f5', minHeight: '100vh' }}>
      <AppBar position="static" sx={{ bgcolor: '#2e7d32' }}>
        <Toolbar>
          <WaterDrop sx={{ mr: 2 }} />
          <Typography variant="h6" component="div" sx={{ flexGrow: 1 }}>
            Greenhouse Monitor
          </Typography>
          <Chip 
            icon={isConnected ? <Wifi /> : <WifiOff />}
            label={isConnected ? "Conectado" : "Desconectado"}
            color={isConnected ? "success" : "error"}
            size="small"
            sx={{ mr: 2 }}
          />
          <Typography variant="body2">
            {new Date().toLocaleString('es-AR', { hour12: false })}
          </Typography>
        </Toolbar>
      </AppBar>

      <Container maxWidth="lg" sx={{ mt: 4, mb: 4 }}>
        {error && (
          <Alert severity="error" sx={{ mb: 3 }}>
            {error}
          </Alert>
        )}

        {/* Sensor Cards */}
        <Grid container spacing={3} sx={{ mb: 4 }}>
          <Grid item xs={12} md={4}>
            <SensorCard
              title="Temperatura"
              value={sensorData?.temperature || 0}
              unit="°C"
              icon={<Thermostat />}
              color="#ef5350"
              errorCount={sensorData?.temp_errors || 0}
            />
          </Grid>
          <Grid item xs={12} md={4}>
            <SensorCard
              title="Humedad"
              value={sensorData?.humidity || 0}
              unit="%"
              icon={<Opacity />}
              color="#42a5f5"
              errorCount={sensorData?.humidity_errors || 0}
            />
          </Grid>
          <Grid item xs={12} md={4}>
            <SensorCard
              title="Humedad Suelo"
              value={sensorData?.soil_moisture || 0}
              unit="%"
              icon={<WaterDrop />}
              color="#8d6e63"
            />
          </Grid>
        </Grid>

        {/* Relay Controls */}
        <Paper sx={{ p: 3 }}>
          <Typography variant="h6" gutterBottom sx={{ mb: 3 }}>
            Control de Relés
          </Typography>
          <Grid container spacing={2}>
            {relayStates.map((relay) => (
              <Grid item xs={12} sm={6} md={3} key={relay.relay_id}>
                <RelayControl 
                  relay={relay} 
                  onToggle={handleRelayToggle}
                />
              </Grid>
            ))}
          </Grid>
        </Paper>

        {/* Sensor Chart */}
        <SensorChart />

        {/* Rule Manager */}
        <RuleManager />

        {/* Log Viewer */}
        <LogViewer />

        {/* Last Update */}
        {sensorData && (
          <Box sx={{ mt: 2, textAlign: 'center' }}>
            <Typography variant="caption" color="text.secondary">
              Última actualización: {new Date(sensorData.timestamp).toLocaleString('es-AR', { hour12: false })}
            </Typography>
          </Box>
        )}
      </Container>
    </Box>
  );
}

export default Dashboard;
