# 🌐 Configuración de Puertos - Greenhouse

## 📊 **Resumen de Puertos**

| Entorno | Puerto | Servicio | Acceso | Descripción |
|---------|--------|----------|--------|-------------|
| **Desarrollo** | 3000 | Express | Público | Frontend + API + WebSocket |
| **Desarrollo** | 27017 | MongoDB | Público | Solo para debugging |
| **Producción** | 3000 | Express | Solo localhost | Frontend + API + WebSocket |
| **Producción** | 80/443 | Nginx | Público | Proxy reverso |

## 🔧 **Configuración por Entorno**

### **Desarrollo Local**
```yaml
# docker-compose.yml
services:
  app:
    ports:
      - "3000:3000"    # Express (todo)
  mongodb:
    ports:
      - "27017:27017"  # MongoDB (debug)
```

### **Producción VPS**
```yaml
# docker-compose.prod.yml
services:
  app:
    ports:
      - "127.0.0.1:3000:3000"  # Solo localhost
  mongodb:
    # NO puertos expuestos (solo interno)
```

## 🌐 **Acceso por Entorno**

### **Desarrollo**
- Frontend: http://localhost:3000
- API: http://localhost:3000/api/*
- WebSocket: http://localhost:3000/socket.io/
- MongoDB: localhost:27017 (debug)

### **Producción**
- Frontend: https://reimon.dev/greenhouse/
- API: https://reimon.dev/greenhouse/api/*
- WebSocket: https://reimon.dev/greenhouse/socket.io/
- MongoDB: Solo interno (no accesible)

## 🔒 **Seguridad**

### **Desarrollo**
- ✅ MongoDB expuesto para debugging
- ✅ Express accesible públicamente
- ✅ Sin autenticación

### **Producción**
- ✅ MongoDB solo interno
- ✅ Express solo en localhost
- ✅ Nginx como proxy con SSL
- ✅ Autenticación ESP32 requerida

## 📋 **Checklist de Deployment**

### **VPS Setup**
- [ ] Docker instalado
- [ ] Nginx configurado
- [ ] SSL certificado (Let's Encrypt)
- [ ] Firewall configurado (80, 443)
- [ ] Dominio apuntando al VPS

### **Aplicación**
- [ ] Imagen Docker construida
- [ ] Variables de entorno configuradas
- [ ] MongoDB sin puertos expuestos
- [ ] Express solo en localhost:3000
- [ ] Nginx proxy funcionando

## 🚀 **Comandos de Deployment**

### **VPS**
```bash
# 1. Copiar configuración Nginx
sudo cp nginx-prod.conf /etc/nginx/sites-available/greenhouse
sudo ln -sf /etc/nginx/sites-available/greenhouse /etc/nginx/sites-enabled/

# 2. Test y reload Nginx
sudo nginx -t
sudo systemctl reload nginx

# 3. Deploy aplicación
docker compose -f docker-compose.prod.yml up -d

# 4. Verificar
curl http://127.0.0.1:3000/health
curl https://reimon.dev/greenhouse/health
```

## 🔍 **Troubleshooting**

### **Puerto 3000 no accesible**
```bash
# Verificar contenedor
docker compose ps

# Ver logs
docker compose logs app

# Verificar puerto
netstat -tulpn | grep 3000
```

### **Nginx 502 Bad Gateway**
```bash
# Verificar Express
curl http://127.0.0.1:3000/health

# Ver logs Nginx
sudo tail -f /var/log/nginx/greenhouse_error.log
```

### **MongoDB no conecta**
```bash
# Verificar contenedor
docker compose logs mongodb

# Verificar red interna
docker exec greenhouse-app ping mongodb
```

## 📝 **Notas Importantes**

1. **MongoDB en producción**: Solo accesible internamente
2. **Express en producción**: Solo en localhost:3000
3. **Nginx**: Maneja todo el tráfico público
4. **SSL**: Requerido para producción
5. **Firewall**: Solo puertos 80 y 443 abiertos
