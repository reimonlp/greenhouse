#!/bin/bash
# Greenhouse IoT - Instalación completa automatizada
# Ejecutar en VPS Ubuntu 24.04

set -e
trap 'echo "❌ Error en línea $LINENO"' ERR

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║     🌱 GREENHOUSE IoT - Instalación Automática en VPS         ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# ============================================================
# PASO 1: Actualizar sistema
# ============================================================
echo "📦 [1/6] Actualizando sistema..."
apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get upgrade -y -qq
apt-get install -y -qq curl gnupg wget software-properties-common

# ============================================================
# PASO 2: Instalar MongoDB 8.0
# ============================================================
echo "🍃 [2/6] Instalando MongoDB 8.0..."
if ! command -v mongod &> /dev/null; then
    curl -fsSL https://www.mongodb.org/static/pgp/server-8.0.asc | \
      gpg -o /usr/share/keyrings/mongodb-server-8.0.gpg --dearmor 2>/dev/null
    
    echo "deb [ arch=amd64,arm64 signed-by=/usr/share/keyrings/mongodb-server-8.0.gpg ] https://repo.mongodb.org/apt/ubuntu noble/mongodb-org/8.0 multiverse" | \
      tee /etc/apt/sources.list.d/mongodb-org-8.0.list > /dev/null
    
    apt-get update -qq
    apt-get install -y -qq mongodb-org
    
    systemctl start mongod
    systemctl enable mongod
    echo "   ✓ MongoDB instalado y ejecutándose"
else
    echo "   ✓ MongoDB ya está instalado"
fi

sleep 3

# ============================================================
# PASO 3: Configurar MongoDB
# ============================================================
echo "🔐 [3/6] Configurando MongoDB..."

# Crear usuario admin si no existe
mongosh --quiet --eval '
try {
  use admin
  const user = db.getUser("greenhouse_admin")
  if (!user) {
    db.createUser({
      user: "greenhouse_admin",
      pwd: "GH2025_SecurePass!",
      roles: [
        {role: "userAdminAnyDatabase", db: "admin"},
        {role: "readWriteAnyDatabase", db: "admin"}
      ]
    })
    print("   ✓ Usuario greenhouse_admin creado")
  } else {
    print("   ✓ Usuario greenhouse_admin ya existe")
  }
} catch(e) {
  print("   ⚠ Error creando usuario:", e)
}
' 2>/dev/null

# Habilitar autenticación
if ! grep -q "authorization: enabled" /etc/mongod.conf; then
    sed -i 's/#security:/security:\n  authorization: enabled/' /etc/mongod.conf
    systemctl restart mongod
    sleep 3
    echo "   ✓ Autenticación habilitada"
fi

# Crear database y colecciones
mongosh --quiet -u greenhouse_admin -p 'GH2025_SecurePass!' --authenticationDatabase admin --eval '
use greenhouse
db.createCollection("sensor_readings")
db.createCollection("relay_states")
db.createCollection("rules")
db.createCollection("system_logs")

db.sensor_readings.createIndex({timestamp: -1})
db.sensor_readings.createIndex({device_id: 1, timestamp: -1})
db.relay_states.createIndex({relay_id: 1, timestamp: -1})
db.system_logs.createIndex({timestamp: -1})
db.rules.createIndex({relay_id: 1})

print("   ✓ Database greenhouse configurada con índices")
' 2>/dev/null

# ============================================================
# PASO 4: Instalar Node.js 20 LTS
# ============================================================
echo "🟢 [4/6] Instalando Node.js 20 LTS..."
if ! command -v node &> /dev/null; then
    curl -fsSL https://deb.nodesource.com/setup_20.x | bash - > /dev/null 2>&1
    apt-get install -y -qq nodejs
    npm install -g pm2 > /dev/null 2>&1
    echo "   ✓ Node.js $(node --version) instalado"
    echo "   ✓ npm $(npm --version) instalado"
    echo "   ✓ PM2 instalado"
else
    echo "   ✓ Node.js ya está instalado: $(node --version)"
    if ! command -v pm2 &> /dev/null; then
        npm install -g pm2 > /dev/null 2>&1
        echo "   ✓ PM2 instalado"
    fi
fi

# ============================================================
# PASO 5: Instalar nginx
# ============================================================
echo "🌐 [5/6] Instalando nginx..."
if ! command -v nginx &> /dev/null; then
    apt-get install -y -qq nginx
    systemctl enable nginx
    echo "   ✓ nginx instalado"
else
    echo "   ✓ nginx ya está instalado"
fi

# ============================================================
# PASO 6: Configurar firewall
# ============================================================
echo "🔥 [6/6] Configurando firewall..."
if command -v ufw &> /dev/null; then
    ufw --force enable > /dev/null 2>&1
    ufw allow 5591/tcp > /dev/null 2>&1  # SSH
    ufw allow 80/tcp > /dev/null 2>&1    # HTTP
    ufw allow 443/tcp > /dev/null 2>&1   # HTTPS
    echo "   ✓ Firewall configurado (puertos: 5591, 80, 443)"
fi

# ============================================================
# RESUMEN
# ============================================================
echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║                    ✅ INSTALACIÓN COMPLETA                     ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "📊 Sistema instalado:"
echo "   • MongoDB: $(mongod --version | head -1 | awk '{print $3}')"
echo "   • Node.js: $(node --version)"
echo "   • npm: $(npm --version)"
echo "   • PM2: $(pm2 --version)"
echo "   • nginx: $(nginx -v 2>&1 | cut -d'/' -f2)"
echo ""
echo "🔐 Credenciales MongoDB:"
echo "   • Usuario: greenhouse_admin"
echo "   • Password: GH2025_SecurePass!"
echo "   • Database: greenhouse"
echo "   • URI: mongodb://greenhouse_admin:GH2025_SecurePass!@localhost:27017/greenhouse?authSource=admin"
echo ""
echo "📂 Próximo paso: Subir el código del backend"
echo ""
