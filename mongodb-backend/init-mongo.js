// Inicializar base de datos y colecciones para el invernadero
db = db.getSiblingDB('invernadero');

// Crear colecciones con índices
db.createCollection('system_logs');
db.createCollection('sensor_data');
db.createCollection('statistics');

// Índices para optimizar queries del ESP32
db.system_logs.createIndex({ timestamp: -1 });
db.system_logs.createIndex({ level: 1, timestamp: -1 });

db.sensor_data.createIndex({ timestamp: -1 });

db.statistics.createIndex({ timestamp: -1 });

print('✅ Base de datos invernadero inicializada con colecciones e índices');
