import { Card, CardContent, Typography, Box, Chip } from '@mui/material';
import { TrendingUp, TrendingDown, Warning } from '@mui/icons-material';

function SensorCard({ title, value, unit, icon, color, errorCount = 0 }) {
  const formattedValue = typeof value === 'number' ? value.toFixed(1) : '0.0';
  
  // Determine if value is in good range (simplified logic)
  const getStatusColor = () => {
    // If there are sensor errors, show warning color
    if (errorCount > 0) {
      return '#ff9800'; // Orange warning color
    }
    
    if (title === 'Temperatura') {
      return value >= 15 && value <= 30 ? '#4caf50' : '#ff9800';
    } else if (title === 'Humedad') {
      return value >= 40 && value <= 70 ? '#4caf50' : '#ff9800';
    } else if (title === 'Humedad Suelo') {
      return value >= 30 && value <= 70 ? '#4caf50' : '#ff9800';
    }
    return color;
  };

  return (
    <Card 
      sx={{ 
        height: '100%',
        transition: 'transform 0.2s, box-shadow 0.2s',
        '&:hover': {
          transform: 'translateY(-4px)',
          boxShadow: 6
        }
      }}
    >
      <CardContent>
        <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
          <Typography variant="h6" color="text.secondary">
            {title}
          </Typography>
          <Box sx={{ color: color, display: 'flex', alignItems: 'center' }}>
            {icon}
          </Box>
        </Box>
        
        <Box sx={{ display: 'flex', alignItems: 'baseline', mb: 1 }}>
          <Typography 
            variant="h3" 
            component="div" 
            sx={{ fontWeight: 'bold', color: getStatusColor() }}
          >
            {formattedValue}
          </Typography>
          <Typography variant="h5" color="text.secondary" sx={{ ml: 1 }}>
            {unit}
          </Typography>
        </Box>

        {/* Status indicator */}
        <Box sx={{ display: 'flex', alignItems: 'center', mt: 1 }}>
          {errorCount >= 3 ? (
            <Chip 
              icon={<Warning />} 
              label="Sensor Defectuoso" 
              color="error" 
              size="small"
              sx={{ fontWeight: 'bold' }}
            />
          ) : errorCount > 0 ? (
            <Chip 
              icon={<Warning />} 
              label={`${errorCount} error${errorCount > 1 ? 'es' : ''} detectado${errorCount > 1 ? 's' : ''}`} 
              color="warning" 
              size="small"
              variant="outlined"
            />
          ) : value > 0 ? (
            <>
              <TrendingUp sx={{ fontSize: 16, color: getStatusColor(), mr: 0.5 }} />
              <Typography variant="caption" sx={{ color: getStatusColor() }}>
                Monitoreo activo
              </Typography>
            </>
          ) : (
            <Typography variant="caption" color="text.secondary">
              Sin datos
            </Typography>
          )}
        </Box>
      </CardContent>
    </Card>
  );
}

export default SensorCard;
