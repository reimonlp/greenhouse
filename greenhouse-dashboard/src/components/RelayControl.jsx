import { useState } from 'react';
import { 
  Card, 
  CardContent, 
  Typography, 
  Switch, 
  Box,
  Chip,
  CircularProgress,
  IconButton,
  Tooltip
} from '@mui/material';
import { 
  Lightbulb, 
  Air, 
  Water, 
  Whatshot,
  AutoMode,
  TouchApp
} from '@mui/icons-material';
import { setRelayState } from '../services/api';

const RELAY_NAMES = {
  0: { name: 'Luces', icon: <Lightbulb /> },
  1: { name: 'Ventilador', icon: <Air /> },
  2: { name: 'Bomba', icon: <Water /> },
  3: { name: 'Calefactor', icon: <Whatshot /> }
};

function RelayControl({ relay, onToggle }) {
  const [loading, setLoading] = useState(false);
  const relayInfo = RELAY_NAMES[relay.relay_id] || { name: `Relé ${relay.relay_id}`, icon: <Lightbulb /> };

  const handleToggle = async () => {
    setLoading(true);
    try {
      const newState = !relay.state;
      await setRelayState(relay.relay_id, newState, 'manual', 'user');
      
      // Notify parent to refresh states
      if (onToggle) {
        onToggle();
      }
    } catch (error) {
      console.error('Error toggling relay:', error);
      alert('Error al cambiar el estado del relé');
    } finally {
      setLoading(false);
    }
  };

  const handleModeToggle = async () => {
    setLoading(true);
    try {
      const newMode = relay.mode === 'manual' ? 'auto' : 'manual';
      // Mantener el estado actual pero cambiar el modo
      await setRelayState(relay.relay_id, relay.state, newMode, newMode === 'manual' ? 'user' : 'system');
      
      // Notify parent to refresh states
      if (onToggle) {
        onToggle();
      }
    } catch (error) {
      console.error('Error changing relay mode:', error);
      alert('Error al cambiar el modo del relé');
    } finally {
      setLoading(false);
    }
  };

  return (
    <Card 
      sx={{ 
        height: '100%',
        bgcolor: relay.state ? 'rgba(46, 125, 50, 0.1)' : 'background.paper',
        border: relay.state ? '2px solid #2e7d32' : '1px solid #e0e0e0',
        transition: 'all 0.3s'
      }}
    >
      <CardContent>
        <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
          <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
            <Box sx={{ color: relay.state ? '#2e7d32' : '#757575' }}>
              {relayInfo.icon}
            </Box>
            <Typography variant="subtitle1" fontWeight="medium">
              {relayInfo.name}
            </Typography>
          </Box>
          
          {loading ? (
            <CircularProgress size={24} />
          ) : (
            <Switch
              checked={relay.state}
              onChange={handleToggle}
              color="success"
              disabled={loading}
            />
          )}
        </Box>

        <Box sx={{ display: 'flex', gap: 1, flexWrap: 'wrap', alignItems: 'center' }}>
          <Chip
            label={relay.state ? 'ENCENDIDO' : 'APAGADO'}
            size="small"
            color={relay.state ? 'success' : 'default'}
            sx={{ fontWeight: 'bold' }}
          />
          <Chip
            label={relay.mode === 'manual' ? 'Manual' : 'Auto'}
            size="small"
            variant="outlined"
            color={relay.mode === 'manual' ? 'warning' : 'primary'}
            icon={relay.mode === 'manual' ? <TouchApp /> : <AutoMode />}
            sx={{ fontSize: '0.7rem' }}
          />
          <Tooltip title={relay.mode === 'manual' ? 'Cambiar a modo Automático' : 'Cambiar a modo Manual'}>
            <IconButton 
              size="small" 
              onClick={handleModeToggle}
              disabled={loading}
              color={relay.mode === 'manual' ? 'primary' : 'warning'}
            >
              {relay.mode === 'manual' ? <AutoMode fontSize="small" /> : <TouchApp fontSize="small" />}
            </IconButton>
          </Tooltip>
        </Box>

        {relay.timestamp && (
          <Typography variant="caption" color="text.secondary" sx={{ mt: 1, display: 'block' }}>
            Actualizado: {new Date(relay.timestamp).toLocaleTimeString('es-AR', { hour12: false })}
          </Typography>
        )}
      </CardContent>
    </Card>
  );
}

export default RelayControl;
