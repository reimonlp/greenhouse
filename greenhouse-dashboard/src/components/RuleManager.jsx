import { useState, useEffect } from 'react';
import webSocketService from '../services/websocket';
import {
  Box,
  Paper,
  Typography,
  Button,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  IconButton,
  Dialog,
  DialogTitle,
  DialogContent,
  DialogActions,
  TextField,
  FormControl,
  InputLabel,
  Select,
  MenuItem,
  Switch,
  FormControlLabel,
  Alert,
  Chip
} from '@mui/material';
import { Delete, Add, Edit } from '@mui/icons-material';

const RELAY_NAMES = ['Luces', 'Ventilador', 'Bomba', 'Calefactor'];
const OPERATORS = ['>', '<', '>=', '<=', '==', '!='];
const DAYS_OF_WEEK = ['Dom', 'Lun', 'Mar', 'Mié', 'Jue', 'Vie', 'Sáb'];

function RuleManager() {
  const handleCloseDialog = () => {
    setOpenDialog(false);
    setEditingRule(null);
    setNewRule({
      name: '',
      relay_id: 0,
      enabled: true,
      rule_type: 'sensor',
      condition: {
        sensor: 'temperature',
        operator: '>',
        threshold: 25
      },
      schedule: {
        time: '06:00',
        days: [0, 1, 2, 3, 4, 5, 6]
      },
      action: 'on'
    });
  };
  const [rules, setRules] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [openDialog, setOpenDialog] = useState(false);
  const [editingRule, setEditingRule] = useState(null);
  const [ruleType, setRuleType] = useState('sensor');
  const [newRule, setNewRule] = useState({
    name: '',
    relay_id: 0,
    enabled: true,
    rule_type: 'sensor',
    condition: {
      sensor: 'temperature',
      operator: '>',
      threshold: 25
    },
    schedule: {
      time: '06:00',
      days: [0, 1, 2, 3, 4, 5, 6]
    },
    action: 'on'
  });

  useEffect(() => {
    setLoading(true);
  webSocketService.emitToServer('rule:list');
    const unsubscribe = webSocketService.on('rule:list', (response) => {
      if (response.success) {
        setRules(response.data);
      }
      setLoading(false);
    });
    return () => unsubscribe();
  }, []);

  const handleOpenDialog = () => {
    setEditingRule(null);
    setRuleType('sensor');
    setOpenDialog(true);
  };

  const handleOpenEditDialog = (rule) => {
    setEditingRule(rule);
    setRuleType(rule.rule_type || 'sensor');
    setNewRule({
      name: rule.name || '',
      relay_id: rule.relay_id,
      enabled: rule.enabled,
      rule_type: rule.rule_type || 'sensor',
      condition: rule.condition || {
        sensor: 'temperature',
        operator: '>',
        threshold: 25
      },
      schedule: rule.schedule || {
        time: '06:00',
        days: [0, 1, 2, 3, 4, 5, 6]
      },
      action: rule.action || 'on'
    });
  };

  const handleSaveRule = () => {
    setLoading(true);
    if (editingRule) {
      webSocketService.emitToServer('rule:update', { ruleId: editingRule._id, ruleData: newRule });
      const unsubscribe = webSocketService.on('rule:updated', (response) => {
        if (response.success) {
          webSocketService.emitToServer('rule:list');
        }
        setLoading(false);
        handleCloseDialog();
        unsubscribe();
      });
    } else {
      webSocketService.emitToServer('rule:create', newRule);
      const unsubscribe = webSocketService.on('rule:created', (response) => {
        if (response.success) {
          webSocketService.emitToServer('rule:list');
        }
        setLoading(false);
        handleCloseDialog();
        unsubscribe();
      });
    }
  };

  const handleDeleteRule = (ruleId) => {
    setLoading(true);
  webSocketService.emitToServer('rule:delete', { ruleId });
    const unsubscribe = webSocketService.on('rule:deleted', (response) => {
      if (response.success) {
  webSocketService.emitToServer('rule:list');
      }
      setLoading(false);
      unsubscribe();
    });
  };

  const getSensorLabel = (sensor) => {
    const labels = {
      temperature: 'Temperatura',
      humidity: 'Humedad',
      soil_moisture: 'Humedad Suelo'
    };
    return labels[sensor] || sensor;
  };

  return (
    <Paper sx={{ p: 3, mt: 3 }}>
      <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 3 }}>
        <Typography variant="h6">
          Reglas de Automatización
        </Typography>
        <Button
          variant="contained"
          color="primary"
          startIcon={<Add />}
          onClick={handleOpenDialog}
        >
          Nueva Regla
        </Button>
      </Box>

      {error && (
        <Alert severity="error" sx={{ mb: 2 }}>
          {error}
        </Alert>
      )}

      <TableContainer>
        <Table>
          <TableHead>
            <TableRow>
              <TableCell>Nombre</TableCell>
              <TableCell>Relé</TableCell>
              <TableCell>Condición</TableCell>
              <TableCell>Acción</TableCell>
              <TableCell>Estado</TableCell>
              <TableCell align="right">Acciones</TableCell>
            </TableRow>
          </TableHead>
          <TableBody>
            {rules.length === 0 ? (
              <TableRow>
                <TableCell colSpan={6} align="center">
                  <Typography color="text.secondary">
                    No hay reglas configuradas
                  </Typography>
                </TableCell>
              </TableRow>
            ) : (
              rules.map((rule) => (
                <TableRow key={rule._id}>
                  <TableCell>
                    {rule.name || 'Sin nombre'}
                    <Chip 
                      label={rule.rule_type === 'time' ? 'Horario' : 'Sensor'} 
                      size="small" 
                      sx={{ ml: 1 }}
                      variant="outlined"
                    />
                  </TableCell>
                  <TableCell>{RELAY_NAMES[rule.relay_id]}</TableCell>
                  <TableCell>
                    {rule.rule_type === 'time' 
                      ? `${rule.schedule?.time || ''} (${rule.schedule?.days?.map(d => DAYS_OF_WEEK[d]).join(', ') || ''})`
                      : `${getSensorLabel(rule.condition?.sensor)} ${rule.condition?.operator} ${rule.condition?.threshold}`
                    }
                  </TableCell>
                  <TableCell>
                    <Chip
                      label={rule.action === 'on' || rule.action === 'turn_on' ? 'Encender' : 'Apagar'}
                      size="small"
                      color={rule.action === 'on' || rule.action === 'turn_on' ? 'success' : 'default'}
                    />
                  </TableCell>
                  <TableCell>
                    <Chip
                      label={rule.enabled ? 'Activa' : 'Inactiva'}
                      size="small"
                      color={rule.enabled ? 'primary' : 'default'}
                      variant={rule.enabled ? 'filled' : 'outlined'}
                    />
                  </TableCell>
                  <TableCell align="right">
                    <IconButton
                      size="small"
                      color="primary"
                      onClick={() => handleOpenEditDialog(rule)}
                      sx={{ mr: 1 }}
                    >
                      <Edit />
                    </IconButton>
                    <IconButton
                      size="small"
                      color="error"
                      onClick={() => handleDeleteRule(rule._id)}
                    >
                      <Delete />
                    </IconButton>
                  </TableCell>
                </TableRow>
              ))
            )}
          </TableBody>
        </Table>
      </TableContainer>

      {/* Create/Edit Rule Dialog */}
      <Dialog open={openDialog} onClose={handleCloseDialog} maxWidth="sm" fullWidth>
        <DialogTitle>
          {editingRule ? 'Editar Regla' : 'Nueva Regla de Automatización'}
        </DialogTitle>
        <DialogContent>
          <Box sx={{ display: 'flex', flexDirection: 'column', gap: 2, mt: 2 }}>
            <TextField
              label="Nombre de la regla"
              value={newRule.name}
              onChange={(e) => setNewRule({ ...newRule, name: e.target.value })}
              fullWidth
            />

            <FormControl fullWidth>
              <InputLabel>Tipo de Regla</InputLabel>
              <Select
                value={ruleType}
                label="Tipo de Regla"
                onChange={(e) => {
                  setRuleType(e.target.value);
                  setNewRule({ ...newRule, rule_type: e.target.value });
                }}
                disabled={!!editingRule}
              >
                <MenuItem value="sensor">Basada en Sensor</MenuItem>
                <MenuItem value="time">Basada en Horario</MenuItem>
              </Select>
            </FormControl>

            <FormControl fullWidth>
              <InputLabel>Relé</InputLabel>
              <Select
                value={newRule.relay_id}
                label="Relé"
                onChange={(e) => setNewRule({ ...newRule, relay_id: e.target.value })}
              >
                {RELAY_NAMES.map((name, idx) => (
                  <MenuItem key={idx} value={idx}>{name}</MenuItem>
                ))}
              </Select>
            </FormControl>

            {ruleType === 'sensor' ? (
              <>
                <FormControl fullWidth>
                  <InputLabel>Sensor</InputLabel>
                  <Select
                    value={newRule.condition.sensor}
                    label="Sensor"
                    onChange={(e) => setNewRule({
                      ...newRule,
                      condition: { ...newRule.condition, sensor: e.target.value }
                    })}
                  >
                    <MenuItem value="temperature">Temperatura</MenuItem>
                    <MenuItem value="humidity">Humedad</MenuItem>
                    <MenuItem value="soil_moisture">Humedad Suelo</MenuItem>
                  </Select>
                </FormControl>

                <Box sx={{ display: 'flex', gap: 2 }}>
                  <FormControl sx={{ minWidth: 100 }}>
                    <InputLabel>Operador</InputLabel>
                    <Select
                      value={newRule.condition.operator}
                      label="Operador"
                      onChange={(e) => setNewRule({
                        ...newRule,
                        condition: { ...newRule.condition, operator: e.target.value }
                      })}
                    >
                      {OPERATORS.map((op) => (
                        <MenuItem key={op} value={op}>{op}</MenuItem>
                      ))}
                    </Select>
                  </FormControl>

                  <TextField
                    label="Valor"
                    type="number"
                    value={newRule.condition.threshold}
                    onChange={(e) => setNewRule({
                      ...newRule,
                      condition: { ...newRule.condition, threshold: parseFloat(e.target.value) }
                    })}
                    fullWidth
                  />
                </Box>
              </>
            ) : (
              <>
                <TextField
                  label="Hora (HH:MM)"
                  type="time"
                  value={newRule.schedule.time}
                  onChange={(e) => setNewRule({
                    ...newRule,
                    schedule: { ...newRule.schedule, time: e.target.value }
                  })}
                  fullWidth
                  InputLabelProps={{ shrink: true }}
                />

                <FormControl fullWidth>
                  <InputLabel>Días de la semana</InputLabel>
                  <Select
                    multiple
                    value={newRule.schedule.days}
                    label="Días de la semana"
                    onChange={(e) => setNewRule({
                      ...newRule,
                      schedule: { ...newRule.schedule, days: e.target.value }
                    })}
                    renderValue={(selected) => selected.map(d => DAYS_OF_WEEK[d]).join(', ')}
                  >
                    {DAYS_OF_WEEK.map((day, idx) => (
                      <MenuItem key={idx} value={idx}>
                        <Switch checked={newRule.schedule.days.includes(idx)} />
                        {day}
                      </MenuItem>
                    ))}
                  </Select>
                </FormControl>
              </>
            )}

            <FormControl fullWidth>
              <InputLabel>Acción</InputLabel>
              <Select
                value={newRule.action}
                label="Acción"
                onChange={(e) => setNewRule({ ...newRule, action: e.target.value })}
              >
                <MenuItem value="on">Encender</MenuItem>
                <MenuItem value="off">Apagar</MenuItem>
              </Select>
            </FormControl>

            <FormControlLabel
              control={
                <Switch
                  checked={newRule.enabled}
                  onChange={(e) => setNewRule({ ...newRule, enabled: e.target.checked })}
                />
              }
              label="Regla activa"
            />
          </Box>
        </DialogContent>
        <DialogActions>
          <Button onClick={handleCloseDialog}>Cancelar</Button>
          <Button onClick={handleSaveRule} variant="contained" color="primary">
            {editingRule ? 'Actualizar' : 'Crear'}
          </Button>
        </DialogActions>
      </Dialog>
    </Paper>
  );
}
export default RuleManager;
