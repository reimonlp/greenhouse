import { useState, useEffect } from 'react';
import {
  Paper,
  Typography,
  Box,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Chip,
  FormControl,
  InputLabel,
  Select,
  MenuItem,
  TextField,
  IconButton,
  Collapse
} from '@mui/material';
import { 
  Info, 
  Warning, 
  Error as ErrorIcon, 
  BugReport,
  Refresh,
  ExpandMore,
  ExpandLess
} from '@mui/icons-material';
import webSocketService from '../services/websocket';

const LOG_LEVELS = {
  info: { color: 'info', icon: <Info fontSize="small" /> },
  warning: { color: 'warning', icon: <Warning fontSize="small" /> },
  error: { color: 'error', icon: <ErrorIcon fontSize="small" /> },
  debug: { color: 'default', icon: <BugReport fontSize="small" /> }
};

const LOG_SOURCES = ['all', 'esp32', 'api', 'system'];

function LogViewer() {
  const [logs, setLogs] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [filterLevel, setFilterLevel] = useState('all');
  const [filterSource, setFilterSource] = useState('all');
  const [expandedRows, setExpandedRows] = useState({});

  useEffect(() => {
    setLoading(true);
    // Solicitar logs por WebSocket
    webSocketService.socket.emit('log:list', {
      limit: 50,
      level: filterLevel === 'all' ? undefined : filterLevel,
      source: filterSource === 'all' ? undefined : filterSource
    });
    // Escuchar respuesta
    const unsubscribe = webSocketService.on('log:list', (response) => {
      if (response.success) {
        setLogs(response.data);
      }
      setLoading(false);
    });
    // Auto refresh cada 10 segundos
    const interval = setInterval(() => {
      webSocketService.socket.emit('log:list', {
        limit: 50,
        level: filterLevel === 'all' ? undefined : filterLevel,
        source: filterSource === 'all' ? undefined : filterSource
      });
    }, 10000);
    return () => {
      clearInterval(interval);
      unsubscribe();
    };
  }, [filterLevel, filterSource]);

  useEffect(() => {
    fetchLogs();
    
    // Auto refresh every 10 seconds
    const interval = setInterval(fetchLogs, 10000);
    
    return () => clearInterval(interval);
  }, [filterLevel, filterSource]);

  const handleToggleRow = (logId) => {
    setExpandedRows(prev => ({
      ...prev,
      [logId]: !prev[logId]
    }));
  };

  const formatTimestamp = (timestamp) => {
    const date = new Date(timestamp);
    return date.toLocaleString('es-AR', {
      day: '2-digit',
      month: '2-digit',
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
      hour12: false
    });
  };

  return (
    <Paper sx={{ p: 3, mt: 3 }}>
      <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 3 }}>
        <Typography variant="h6">
          Registro de Sistema
        </Typography>
        <Box sx={{ display: 'flex', gap: 2, alignItems: 'center' }}>
          <FormControl size="small" sx={{ minWidth: 120 }}>
            <InputLabel>Nivel</InputLabel>
            <Select
              value={filterLevel}
              label="Nivel"
              onChange={(e) => setFilterLevel(e.target.value)}
            >
              <MenuItem value="all">Todos</MenuItem>
              <MenuItem value="info">Info</MenuItem>
              <MenuItem value="warning">Warning</MenuItem>
              <MenuItem value="error">Error</MenuItem>
              <MenuItem value="debug">Debug</MenuItem>
            </Select>
          </FormControl>
          
          <FormControl size="small" sx={{ minWidth: 120 }}>
            <InputLabel>Fuente</InputLabel>
            <Select
              value={filterSource}
              label="Fuente"
              onChange={(e) => setFilterSource(e.target.value)}
            >
              <MenuItem value="all">Todas</MenuItem>
              <MenuItem value="esp32">ESP32</MenuItem>
              <MenuItem value="api">API</MenuItem>
              <MenuItem value="system">Sistema</MenuItem>
            </Select>
          </FormControl>

          <IconButton onClick={fetchLogs} color="primary">
            <Refresh />
          </IconButton>
        </Box>
      </Box>

      <TableContainer sx={{ maxHeight: 500 }}>
        <Table stickyHeader size="small">
          <TableHead>
            <TableRow>
              <TableCell width={40}></TableCell>
              <TableCell>Nivel</TableCell>
              <TableCell>Fuente</TableCell>
              <TableCell>Mensaje</TableCell>
              <TableCell>Fecha/Hora</TableCell>
            </TableRow>
          </TableHead>
          <TableBody>
            {logs.length === 0 ? (
              <TableRow>
                <TableCell colSpan={5} align="center">
                  <Typography color="text.secondary">
                    No hay logs disponibles
                  </Typography>
                </TableCell>
              </TableRow>
            ) : (
              logs.map((log) => {
                const levelInfo = LOG_LEVELS[log.level] || LOG_LEVELS.info;
                const isExpanded = expandedRows[log._id];
                
                return (
                  <>
                    <TableRow 
                      key={log._id}
                      hover
                      sx={{ cursor: log.metadata ? 'pointer' : 'default' }}
                      onClick={() => log.metadata && handleToggleRow(log._id)}
                    >
                      <TableCell>
                        {log.metadata && (
                          <IconButton size="small">
                            {isExpanded ? <ExpandLess /> : <ExpandMore />}
                          </IconButton>
                        )}
                      </TableCell>
                      <TableCell>
                        <Chip
                          icon={levelInfo.icon}
                          label={log.level.toUpperCase()}
                          size="small"
                          color={levelInfo.color}
                          sx={{ minWidth: 90 }}
                        />
                      </TableCell>
                      <TableCell>
                        <Chip
                          label={log.source}
                          size="small"
                          variant="outlined"
                        />
                      </TableCell>
                      <TableCell>
                        <Typography variant="body2" noWrap={!isExpanded}>
                          {log.message}
                        </Typography>
                      </TableCell>
                      <TableCell>
                        <Typography variant="caption" color="text.secondary">
                          {formatTimestamp(log.timestamp)}
                        </Typography>
                      </TableCell>
                    </TableRow>
                    
                    {log.metadata && (
                      <TableRow>
                        <TableCell colSpan={5} sx={{ py: 0, borderBottom: isExpanded ? 1 : 0 }}>
                          <Collapse in={isExpanded} timeout="auto" unmountOnExit>
                            <Box sx={{ p: 2, bgcolor: 'grey.50' }}>
                              <Typography variant="caption" fontWeight="bold">
                                Metadata:
                              </Typography>
                              <pre style={{ 
                                margin: '8px 0 0 0', 
                                fontSize: '0.75rem',
                                overflow: 'auto',
                                backgroundColor: '#fff',
                                padding: '8px',
                                borderRadius: '4px'
                              }}>
                                {JSON.stringify(log.metadata, null, 2)}
                              </pre>
                            </Box>
                          </Collapse>
                        </TableCell>
                      </TableRow>
                    )}
                  </>
                );
              })
            )}
          </TableBody>
        </Table>
      </TableContainer>

      <Box sx={{ mt: 2, textAlign: 'center' }}>
        <Typography variant="caption" color="text.secondary">
          Mostrando últimos {logs.length} logs • Actualización automática cada 10s
        </Typography>
      </Box>
    </Paper>
  );
}

export default LogViewer;
