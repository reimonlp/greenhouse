import { Card, CardContent, Typography, Box } from '@mui/material';
import { TrendingUp, TrendingDown } from '@mui/icons-material';

function SensorCard({ title, value, unit, icon, color }) {
  const formattedValue = typeof value === 'number' ? value.toFixed(1) : '0.0';
  
  // Determine if value is in good range (simplified logic)
  const getStatusColor = () => {
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
          {value > 0 ? (
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
