import { useState, useEffect } from 'react';
import {
  Paper,
  Typography,
  Box,
  FormControl,
  InputLabel,
  Select,
  MenuItem,
  ToggleButtonGroup,
  ToggleButton,
  CircularProgress
} from '@mui/material';
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Legend,
  ResponsiveContainer
} from 'recharts';
// import duplicada eliminada
import webSocketService from '../services/websocket';

const TIME_RANGES = {
  '1h': { label: '1 Hora', ms: 60 * 60 * 1000 },
  '6h': { label: '6 Horas', ms: 6 * 60 * 60 * 1000 },
  '24h': { label: '24 Horas', ms: 24 * 60 * 60 * 1000 },
  '7d': { label: '7 Días', ms: 7 * 24 * 60 * 60 * 1000 }
};

function SensorChart() {
  const [data, setData] = useState([]);
  const [loading, setLoading] = useState(true);
  const [timeRange, setTimeRange] = useState('6h');
  const [selectedSensors, setSelectedSensors] = useState(['temperature', 'humidity']);

  useEffect(() => {
    setLoading(true);
  webSocketService.emitToServer('sensor:history', { limit: 2000 });
    const unsubscribe = webSocketService.on('sensor:history', (response) => {
      if (response.success && response.data) {
        const now = Date.now();
        const rangeMs = TIME_RANGES[timeRange].ms;
        const filtered = response.data.filter(reading => {
          const ts = new Date(reading.timestamp).getTime();
          return now - ts <= rangeMs;
        });
        const transformedData = filtered
          .reverse()
          .map(reading => ({
            timestamp: new Date(reading.timestamp).toLocaleTimeString('es-AR', {
              hour: '2-digit',
              minute: '2-digit',
              hour12: false,
              ...(timeRange === '7d' && { day: '2-digit', month: '2-digit' })
            }),
            fullTimestamp: reading.timestamp,
            temperature: reading.temperature,
            humidity: reading.humidity,
            soil_moisture: reading.soil_moisture
          }));
        setData(transformedData);
      }
      setLoading(false);
    });
    return () => unsubscribe();
  }, [timeRange]);

  useEffect(() => {
    const refreshInterval = timeRange === '1h' ? 30000 : 60000;
    const interval = setInterval(() => {
      webSocketService.emitToServer('sensor:history', { limit: 2000 });
    }, refreshInterval);
    return () => clearInterval(interval);
  }, [timeRange]);

  const handleSensorToggle = (event, newSensors) => {
    if (newSensors.length > 0) {
      setSelectedSensors(newSensors);
    }
  };

  const CustomTooltip = ({ active, payload, label }) => {
    if (active && payload && payload.length) {
      return (
        <Paper sx={{ p: 1.5, bgcolor: 'rgba(255, 255, 255, 0.95)' }}>
          <Typography variant="caption" fontWeight="bold">
            {label}
          </Typography>
          {payload.map((entry, index) => (
            <Typography key={index} variant="body2" sx={{ color: entry.color }}>
              {entry.name}: {entry.value?.toFixed(1)} {entry.unit}
            </Typography>
          ))}
        </Paper>
      );
    }
    return null;
  };

  return (
    <Paper sx={{ p: 3, mt: 3 }}>
      <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 3, flexWrap: 'wrap', gap: 2 }}>
        <Typography variant="h6">
          Histórico de Sensores
        </Typography>
        
        <Box sx={{ display: 'flex', gap: 2, alignItems: 'center', flexWrap: 'wrap' }}>
          <ToggleButtonGroup
            value={selectedSensors}
            onChange={handleSensorToggle}
            aria-label="sensor selection"
            size="small"
          >
            <ToggleButton value="temperature" aria-label="temperatura">
              Temp
            </ToggleButton>
            <ToggleButton value="humidity" aria-label="humedad">
              Hum
            </ToggleButton>
            <ToggleButton value="soil_moisture" aria-label="humedad suelo">
              Suelo
            </ToggleButton>
          </ToggleButtonGroup>

          <FormControl size="small" sx={{ minWidth: 120 }}>
            <InputLabel>Rango</InputLabel>
            <Select
              value={timeRange}
              label="Rango"
              onChange={(e) => setTimeRange(e.target.value)}
            >
              {Object.entries(TIME_RANGES).map(([key, value]) => (
                <MenuItem key={key} value={key}>
                  {value.label}
                </MenuItem>
              ))}
            </Select>
          </FormControl>
        </Box>
      </Box>

      {loading ? (
        <Box sx={{ display: 'flex', justifyContent: 'center', alignItems: 'center', height: 400 }}>
          <CircularProgress />
        </Box>
      ) : data.length === 0 ? (
        <Box sx={{ display: 'flex', justifyContent: 'center', alignItems: 'center', height: 400 }}>
          <Typography color="text.secondary">
            No hay datos disponibles para este rango
          </Typography>
        </Box>
      ) : (
        <ResponsiveContainer width="100%" height={400}>
          <LineChart data={data} margin={{ top: 5, right: 30, left: 20, bottom: 5 }}>
            <CartesianGrid strokeDasharray="3 3" />
            <XAxis 
              dataKey="timestamp" 
              style={{ fontSize: '0.75rem' }}
              interval="preserveStartEnd"
            />
            <YAxis style={{ fontSize: '0.75rem' }} />
            <Tooltip content={<CustomTooltip />} />
            <Legend />
            
            {selectedSensors.includes('temperature') && (
              <Line
                type="monotone"
                dataKey="temperature"
                name="Temperatura"
                stroke="#ef5350"
                strokeWidth={2}
                dot={false}
                unit="°C"
              />
            )}
            
            {selectedSensors.includes('humidity') && (
              <Line
                type="monotone"
                dataKey="humidity"
                name="Humedad"
                stroke="#42a5f5"
                strokeWidth={2}
                dot={false}
                unit="%"
              />
            )}
            
            {selectedSensors.includes('soil_moisture') && (
              <Line
                type="monotone"
                dataKey="soil_moisture"
                name="Humedad Suelo"
                stroke="#8d6e63"
                strokeWidth={2}
                dot={false}
                unit="%"
              />
            )}
          </LineChart>
        </ResponsiveContainer>
      )}

      <Box sx={{ mt: 2, textAlign: 'center' }}>
        <Typography variant="caption" color="text.secondary">
          Mostrando {data.length} lecturas • Actualización automática
        </Typography>
      </Box>
    </Paper>
  );
}

export default SensorChart;
