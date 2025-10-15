#!/bin/bash
# Setup script for MongoDB on Ubuntu 24.04
set -e

echo "=== Actualizando sistema ==="
apt-get update
apt-get upgrade -y

echo "=== Instalando dependencias ==="
apt-get install -y curl gnupg

echo "=== Instalando MongoDB 8.0 ==="
curl -fsSL https://www.mongodb.org/static/pgp/server-8.0.asc | \
  gpg -o /usr/share/keyrings/mongodb-server-8.0.gpg --dearmor

echo "deb [ arch=amd64,arm64 signed-by=/usr/share/keyrings/mongodb-server-8.0.gpg ] https://repo.mongodb.org/apt/ubuntu noble/mongodb-org/8.0 multiverse" | \
  tee /etc/apt/sources.list.d/mongodb-org-8.0.list

apt-get update
apt-get install -y mongodb-org

echo "=== Iniciando MongoDB ==="
systemctl start mongod
systemctl enable mongod

echo "=== Esperando a que MongoDB esté listo ==="
sleep 5

echo "=== Creando usuario administrador ==="
mongosh --eval '
use admin
db.createUser({
  user: "greenhouse_admin",
  pwd: "GH2025_SecurePass!",
  roles: [
    {role: "userAdminAnyDatabase", db: "admin"},
    {role: "readWriteAnyDatabase", db: "admin"}
  ]
})
'

echo "=== Habilitando autenticación ==="
sed -i 's/#security:/security:\n  authorization: enabled/' /etc/mongod.conf

echo "=== Reiniciando MongoDB con autenticación ==="
systemctl restart mongod

echo "=== Creando database greenhouse ==="
mongosh -u greenhouse_admin -p 'GH2025_SecurePass!' --authenticationDatabase admin --eval '
use greenhouse
db.createCollection("sensor_readings")
db.createCollection("relay_states")
db.createCollection("rules")
db.createCollection("system_logs")

// Índices para optimizar queries
db.sensor_readings.createIndex({timestamp: -1})
db.sensor_readings.createIndex({device_id: 1, timestamp: -1})
db.relay_states.createIndex({relay_id: 1, timestamp: -1})
db.system_logs.createIndex({timestamp: -1})
db.rules.createIndex({relay_id: 1})

print("✓ Database y colecciones creadas")
'

echo "=== MongoDB configurado exitosamente ==="
echo "Usuario: greenhouse_admin"
echo "Password: GH2025_SecurePass!"
echo "Database: greenhouse"
