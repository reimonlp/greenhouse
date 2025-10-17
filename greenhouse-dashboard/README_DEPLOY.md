# 🌐 Despliegue en reimon.dev/greenhouse

## 📋 Resumen

Este documento describe cómo configurar el dominio `reimon.dev` para servir el frontend y la API del sistema greenhouse.

**URLs finales:**
- 🌍 Frontend: `https://reimon.dev/greenhouse`
- 🔌 API: `https://reimon.dev/greenhouse/api`

---

## 🚀 Guía Rápida de Despliegue

### **Paso 1: Configurar el VPS**

Conecta al VPS y ejecuta el script de configuración:

```bash
# Conectar al VPS
ssh root@reimon.dev

# Copiar archivos de configuración
cd /tmp
# (Copia los archivos nginx-config-reimon.dev y setup-vps.sh al VPS)

# Ejecutar configuración
bash setup-vps.sh

# Configurar SSL
sudo certbot --nginx -d reimon.dev -d www.reimon.dev
```

### **Paso 2: Build y Deploy del Frontend**

Desde tu máquina local:

```bash
cd /home/reimon/greenhouse/greenhouse-dashboard

# Instalar dependencias (si es necesario)
npm install

# Deploy automático
./deploy-domain.sh
```

### **Paso 3: Verificar**

```bash
# Desde tu máquina local
./verify-domain.sh

# O manualmente:
curl -I https://reimon.dev/greenhouse
curl https://reimon.dev/greenhouse/api/health
```

---

## 📁 Archivos Creados

| Archivo | Descripción |
|---------|-------------|
| `vite.config.js` | Configurado con `base: '/greenhouse/'` |
| `.env.production` | URL de API para producción |
| `nginx-config-reimon.dev` | Configuración completa de Nginx |
| `setup-vps.sh` | Script de configuración del VPS |
| `deploy-domain.sh` | Script de deploy automático |
| `verify-domain.sh` | Script de verificación |
| `DOMAIN_SETUP.md` | Documentación detallada |

---

## 🔧 Configuración Técnica

### **Frontend (Vite)**

```javascript
// vite.config.js
export default defineConfig({
  base: '/greenhouse/', // ⚠️ CRÍTICO para rutas
  // ...
})
```

### **Variables de Entorno**

```bash
# .env.production
VITE_API_URL=https://reimon.dev/greenhouse/api
```

### **Nginx**

- Frontend: `/var/www/greenhouse/` → `https://reimon.dev/greenhouse`
- API Proxy: `http://localhost:80/api/` → `https://reimon.dev/greenhouse/api`
- SSL: Let's Encrypt con renovación automática

---

## 📝 Comandos Útiles

### **En el VPS:**

```bash
# Ver logs de Nginx
sudo tail -f /var/log/nginx/reimon.dev.access.log
sudo tail -f /var/log/nginx/reimon.dev.error.log

# Recargar Nginx
sudo nginx -t && sudo systemctl reload nginx

# Ver estado de la API
pm2 list
pm2 logs greenhouse-backend

# Renovar SSL (automático, pero puedes forzar)
sudo certbot renew --dry-run
```

### **En tu máquina local:**

```bash
# Build local (sin deploy)
cd greenhouse-dashboard
npm run build

# Deploy manual
rsync -avz dist/ root@reimon.dev:/var/www/greenhouse/

# Deploy con script
./deploy-domain.sh

# Verificar después de deploy
./verify-domain.sh
```

---

## 🐛 Troubleshooting

### **❌ Error: "404 Not Found" en rutas del frontend**

**Causa:** Nginx no está redirigiendo correctamente a `index.html`

**Solución:**
```nginx
location /greenhouse/ {
    alias /var/www/greenhouse/;
    try_files $uri $uri/ /greenhouse/index.html;  # ⚠️ CRÍTICO
}
```

### **❌ Error: "CORS policy" en la consola del navegador**

**Causa:** Headers CORS no configurados

**Solución:** Verifica en `/etc/nginx/sites-available/reimon.dev`:
```nginx
add_header 'Access-Control-Allow-Origin' '*' always;
```

### **❌ Error: Assets (CSS/JS) no cargan**

**Causa:** Base path incorrecto en Vite

**Solución:**
```javascript
// vite.config.js
base: '/greenhouse/',  // ⚠️ Debe terminar con /
```

### **❌ Error: API no responde (502 Bad Gateway)**

**Causa:** Backend no está corriendo o puerto incorrecto

**Solución:**
```bash
# En el VPS
pm2 list
pm2 restart greenhouse-backend

# Verificar puerto
curl http://localhost:80/api/health
```

### **❌ Error: Certificado SSL inválido**

**Causa:** Certbot no ejecutado o falló

**Solución:**
```bash
sudo certbot --nginx -d reimon.dev -d www.reimon.dev --force-renewal
```

---

## ✅ Checklist de Verificación

Después del deploy, verifica:

- [ ] Frontend carga en `https://reimon.dev/greenhouse`
- [ ] No hay errores 404 en la consola del navegador
- [ ] CSS y JavaScript cargan correctamente
- [ ] API responde en `https://reimon.dev/greenhouse/api/health`
- [ ] Los datos del dashboard se cargan (sensores, relays)
- [ ] SSL muestra el candado verde en el navegador
- [ ] No hay errores CORS en la consola

---

## 🔒 Seguridad

- ✅ **HTTPS obligatorio** (redirect de HTTP a HTTPS)
- ✅ **SSL/TLS 1.2+** únicamente
- ✅ **Headers de seguridad** configurados
- ✅ **CORS** correctamente configurado
- ✅ **Logs habilitados** para auditoría

---

## 📊 Arquitectura Final

```
┌─────────────────────────────────────────────┐
│         https://reimon.dev/greenhouse       │
└──────────────────┬──────────────────────────┘
                   │
         ┌─────────▼─────────┐
         │  Nginx (VPS)      │
         │  Port 443 (HTTPS) │
         └─────┬───────┬─────┘
               │       │
       ┌───────▼──┐  ┌─▼────────────┐
       │ Frontend │  │ API Backend  │
       │ (React)  │  │ (Node.js)    │
       │ /var/www │  │ Port 80      │
       └──────────┘  └──┬───────────┘
                        │
                   ┌────▼────┐
                   │ MongoDB │
                   └─────────┘
```

---

## 📞 Soporte

Si tienes problemas:

1. Revisa los logs de Nginx: `sudo tail -f /var/log/nginx/reimon.dev.error.log`
2. Revisa los logs de PM2: `pm2 logs greenhouse-backend`
3. Verifica la configuración: `sudo nginx -t`
4. Consulta `DOMAIN_SETUP.md` para detalles técnicos completos

---

**¡Listo! 🎉** Tu aplicación greenhouse está ahora en producción en `https://reimon.dev/greenhouse`
