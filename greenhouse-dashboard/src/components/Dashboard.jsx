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
  Chip,
  Button,
  Modal,
  IconButton
} from '@mui/material';
import { Thermostat, Opacity, WaterDrop, WifiOff, Wifi, ShowChart } from '@mui/icons-material';
import SensorCard from './SensorCard';
import RelayControl from './RelayControl';
import RuleManager from './RuleManager';
import SensorChart from './SensorChart';
import LogViewer from './LogViewer';
import { getLatestSensorReading, getRelayStates } from '../services/api';
import { useWebSocket, useSensorUpdates } from '../hooks/useWebSocket';

function Dashboard() {
  // Storm event hook
  const { useStormEvent } = require('../hooks/useWebSocket');
  const stormData = typeof useStormEvent === 'function' ? useStormEvent() : null;
  const [sensorData, setSensorData] = useState(null);
  const [relayStates, setRelayStates] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [chartModalOpen, setChartModalOpen] = useState(false);
  
  // WebSocket hooks
  const { isConnected } = useWebSocket();
  const latestSensor = useSensorUpdates();

  // Actualizar sensores cuando llegan por WebSocket
  useEffect(() => {
    // Solicitar datos iniciales por WebSocket
    setLoading(true);
    webSocketService.socket.emit('sensor:latest');
    const unsubSensor = webSocketService.on('sensor:latest', (response) => {
      if (response.success && response.data) {
        setSensorData(response.data);
      }
      setLoading(false);
    });
    webSocketService.socket.emit('relay:states');
    const unsubRelay = webSocketService.on('relay:states', (response) => {
      if (response.success && response.data) {
        setRelayStates(response.data);
      }
    });
    return () => {
      unsubSensor();
      unsubRelay();
    };
  }, []);

  // Render principal
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
        {stormData && (
          <Alert severity="warning" variant="filled" sx={{ mb: 3 }}>
            <Typography variant="h6">Situación de tormenta detectada</Typography>
            <Typography variant="body1">
              Humedad del sensor: <b>{stormData.sensor_humidity}%</b><br />
              Humedad actual en La Plata: <b>{stormData.ciudad_humidity ? `${stormData.ciudad_humidity}%` : 'No disponible'}</b>
            </Typography>
            {stormData.api_error && (
              <Typography variant="caption" color="error">Error API: {stormData.api_error}</Typography>
            )}
          </Alert>
        )}
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
              value={typeof sensorData?.external_humidity === 'number' && sensorData?.external_humidity >= 0 ? sensorData.external_humidity : sensorData?.humidity || 0}
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

        {/* Chart Modal Button */}
        <Box sx={{ mb: 4, textAlign: 'center' }}>
          <Button
            variant="contained"
            startIcon={<ShowChart />}
            onClick={() => setChartModalOpen(true)}
            sx={{ 
              bgcolor: '#2e7d32',
              '&:hover': { bgcolor: '#1b5e20' },
              px: 4,
              py: 1.5
            }}
          >
            Ver Gráficos de Sensores
          </Button>
        </Box>

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

// Eliminar fragmentos sueltos fuera del return principal

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

      {/* Sensor Chart Modal */}
      <Modal
        open={chartModalOpen}
        onClose={() => setChartModalOpen(false)}
        aria-labelledby="sensor-chart-modal"
        aria-describedby="modal-with-sensor-charts"
      >
        <Box sx={{
          position: 'absolute',
          top: '50%',
          left: '50%',
          transform: 'translate(-50%, -50%)',
          width: '90%',
          maxWidth: '1200px',
          maxHeight: '90vh',
          bgcolor: 'background.paper',
          boxShadow: 24,
          p: 4,
          borderRadius: 2,
          overflow: 'auto'
        }}>
          <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
            <Typography variant="h5" component="h2">
              Gráficos de Sensores
            </Typography>
            <IconButton onClick={() => setChartModalOpen(false)}>
              ✕
            </IconButton>
          </Box>
          <SensorChart />
        </Box>
      </Modal>
    </Box>
  );
}

export default Dashboard;
