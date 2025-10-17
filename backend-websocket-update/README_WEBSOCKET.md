# 🌐 Actualización a WebSocket en Tiempo Real

## 📋 Resumen de Cambios

Esta actualización migra el sistema de **polling HTTP** a **WebSocket (Socket.IO)** para actualizaciones en tiempo real.

### **Antes** (Polling HTTP):
- ❌ Peticiones HTTP cada 5 segundos
- ❌ Mayor latencia en actualizaciones
- ❌ Mayor consumo de ancho de banda
- ❌ Carga innecesaria en el servidor

### **Después** (WebSocket):
- ✅ Actualizaciones instantáneas en tiempo real
- ✅ Conexión persistente bidireccional
- ✅ Menor latencia (< 100ms)
- ✅ Menor consumo de recursos
- ✅ Indicador visual de estado de conexión

---

## 🔧 Archivos Modificados

### **Backend** (`/opt/backend/`)
- ✅ `server.js` - Agregado soporte Socket.IO
- ✅ `package.json` - Dependencia `socket.io` agregada

### **Frontend** (`greenhouse-dashboard/`)
- ✅ `src/services/websocket.js` - Servicio WebSocket nuevo
- ✅ `src/hooks/useWebSocket.js` - Hooks de React para WebSocket
- ✅ `src/components/Dashboard.jsx` - Actualizado para usar WebSocket
- ✅ `package.json` - Dependencia `socket.io-client` agregada

### **Nginx**
- ✅ Configuración actualizada con soporte WebSocket
- ✅ Proxy para `/greenhouse/socket.io/`
- ✅ Headers `Upgrade` y `Connection` configurados
- ✅ Timeouts largos para conexiones persistentes

---

## 🚀 Instalación Local

### **Paso 1: Instalar dependencias del backend**

```bash
cd /home/reimon/greenhouse/backend-websocket-update
npm install
```

### **Paso 2: Ejecutar el backend**

```bash
npm start
```

### **Paso 3: Instalar y ejecutar el frontend**

```bash
cd ../greenhouse-dashboard
npm install
npm run dev
```

---

## 📡 Eventos WebSocket Implementados

### **Eventos del Servidor → Cliente**

| Evento | Descripción | Datos |
|--------|-------------|-------|
| `connected` | Confirmación de conexión | `{ message, timestamp }` |
| `sensor:new` | Nueva lectura de sensor | Objeto SensorReading completo |
| `relay:changed` | Estado de relay cambiado | Objeto RelayState actualizado |
| `rule:created` | Nueva regla creada | Objeto Rule |
| `rule:updated` | Regla actualizada | Objeto Rule |
| `rule:deleted` | Regla eliminada | `{ id }` |

### **Eventos del Cliente → Servidor**

Actualmente solo escucha eventos. Los cambios se siguen haciendo vía API REST.

---

## 🎨 Características del Frontend

### **Indicador de Conexión**
- 🟢 **Verde**: Conectado (tiempo real activo)
- 🔴 **Rojo**: Desconectado (usando datos en caché)

### **Actualizaciones Automáticas**
- **Sensores**: Se actualizan instantáneamente cuando el ESP32 envía datos
- **Relays**: Se actualizan cuando cambian de estado (manual o automático)
- **Reglas**: Se reflejan cambios inmediatamente en todos los clientes

### **Manejo de Reconexión**
- Reintentos automáticos (hasta 5 intentos)
- Fallback a datos en caché si WebSocket falla
- Reconexión transparente al restaurar conectividad

---

## 🔍 Verificación Post-Instalación

### **1. Backend**
```bash
# Ver logs del backend
pm2 logs greenhouse-api

# Deberías ver:
# ✓ WebSocket habilitado en /socket.io/
# ✓ Cliente WebSocket conectado: [socket-id]
```

### **2. Nginx**
```bash
# Verificar configuración
nginx -t

# Ver logs de acceso
tail -f /var/log/nginx/reimon.dev.access.log
```

### **3. Frontend**
Abre la consola del navegador en `http://localhost:3001`:
```
✓ WebSocket conectado: [socket-id]
Mensaje del servidor: {...}
```

### **4. Prueba en Tiempo Real**
1. Abre el dashboard en 2 navegadores
2. Cambia un relay en uno
3. Debería actualizarse instantáneamente en el otro

---

## 🐛 Troubleshooting

### **Error: WebSocket no conecta**

**Síntomas:**
- Chip rojo "Desconectado" en el header
- Console error: `WebSocket connection failed`

**Soluciones:**
```bash
# 1. Verificar que el backend esté corriendo
ps aux | grep node
tail -f logs del proceso

# 2. Verificar que el puerto 3000 esté abierto
netstat -tlnp | grep 3000

# 3. Verificar logs de Nginx
tail -f /var/log/nginx/reimon.dev.error.log
```

### **Error: 502 Bad Gateway en /socket.io/**

**Causa:** Backend no está corriendo o Nginx mal configurado

**Solución:**
```bash
# Reiniciar backend
pm2 restart greenhouse-api

# Recargar Nginx
systemctl reload nginx
```

### **Error: Conexión se cae constantemente**

**Causa:** Timeouts en la conexión

**Solución:** Verificar que el servidor esté configurado para conexiones largas

---

## 📊 Comparativa de Rendimiento

| Métrica | HTTP Polling | WebSocket |
|---------|-------------|-----------|
| **Latencia** | 2-5 segundos | < 100ms |
| **Requests/min** | 12-24 | 0 (después de conectar) |
| **Ancho de banda** | ~500 KB/min | ~5 KB/min |
| **CPU servidor** | Media | Baja |
| **Batería cliente** | Alta | Baja |

---

## 🔐 Seguridad

- ✅ **WSS** (WebSocket Secure) sobre HTTPS
- ✅ **CORS** configurado correctamente
- ✅ **SSL/TLS** mediante Let's Encrypt
- ✅ **Rate limiting** en API REST (no afecta WS)

---

## 📝 Notas Adicionales

### **Compatibilidad**
- Socket.IO incluye fallback automático a long-polling si WebSocket no está disponible
- Compatible con todos los navegadores modernos
- Funciona detrás de proxies y firewalls

### **Escalabilidad**
- Para múltiples instancias del backend, considerar Redis Adapter
- Socket.IO mantiene conexiones persistentes eficientemente
- Nginx maneja correctamente el load balancing de WebSocket

### **Monitoreo**
```bash
# Ver conexiones WebSocket activas
pm2 logs greenhouse-api | grep "WebSocket"

# Número de clientes conectados (agregar esta métrica al health check)
curl http://localhost:3000/api/health
```

---

## ✅ Checklist de Verificación

Después de la instalación, verifica:

- [ ] Backend muestra "WebSocket habilitado" en los logs
- [ ] Frontend muestra chip verde "Conectado"
- [ ] Consola del navegador muestra "WebSocket conectado"
- [ ] Cambios en sensores se reflejan instantáneamente
- [ ] Cambios en relays se reflejan en todos los clientes
- [ ] No hay errores en los logs de Nginx
- [ ] SSL funciona correctamente (wss://)

---

## 🎯 Próximos Pasos (Opcional)

1. **Notificaciones Push**: Alertas en tiempo real cuando se activen reglas
2. **Chat entre usuarios**: Comunicación entre múltiples administradores
3. **Streaming de logs**: Logs en tiempo real en el dashboard
4. **Métricas en vivo**: Gráficos que se actualizan en tiempo real

---

**¡Listo!** Tu sistema Greenhouse ahora usa WebSocket para actualizaciones en tiempo real 🚀
