# MongoDB Backend para ESP32 Invernadero

Stack local con MongoDB + API REST para recibir logs y datos de sensores del ESP32.

## Inicio rápido

```bash
# Levantar servicios (MongoDB + API)
docker compose up -d

# Ver logs
docker compose logs -f api

# Parar servicios
docker compose down
```

## Endpoints API

- **POST /api/logs** - Insertar log desde ESP32
  ```json
  {
    "timestamp": 1697260800,
    "level": 3,
    "source": "sensors",
    "message": "DHT11 read error",
    "data": "{\"error\":\"timeout\"}"
  }
  ```

- **GET /api/logs?count=50** - Obtener últimos N logs
  ```json
  {
    "documents": [
      { "timestamp": 1697260800, "level": 3, "source": "sensors", ... }
    ]
  }
  ```

- **POST /api/sensors** - Insertar dato de sensor
- **GET /api/sensors?from=X&to=Y** - Obtener historial

## Configuración ESP32

En `include/secrets.h` o `include/config.h`, reemplazá:

```cpp
// Si el servidor está en tu red local:
#define MONGODB_API_BASE "http://192.168.1.100:3000"  // IP de tu servidor

// O si usás hostname:
#define MONGODB_API_BASE "http://tu-servidor.local:3000"
```

El firmware ya está preparado para hacer POST/GET a estos endpoints.

## Base de datos

- **Base**: `invernadero`
- **Colecciones**:
  - `system_logs` - Logs del sistema (timestamp, level, source, message, data)
  - `sensor_data` - Datos de sensores (timestamp, temperature, humidity, soil_moisture_1/2, valid)
  - `statistics` - Estadísticas agregadas

## Acceso directo a MongoDB

```bash
# Conectar a MongoDB con mongosh
docker exec -it greenhouse-mongo mongosh -u admin -p greenhouse123 --authenticationDatabase admin

# Ver logs
use invernadero
db.system_logs.find().sort({timestamp:-1}).limit(10)
```

## Producción

- Cambiar contraseñas en `docker-compose.yml` (MONGO_INITDB_ROOT_PASSWORD).
- Opcional: agregar autenticación/token en la API si exponés el puerto 3000 a Internet.
- Para HTTPS, ponés un reverse proxy (nginx/Caddy) delante.

## Troubleshooting

- **ESP32 no conecta**: verificá que esté en la misma red y que la IP/hostname sea alcanzable.
- **MongoDB no arranca**: `docker compose logs mongodb` para ver errores.
- **API no responde**: `docker compose logs api` y verificá que MongoDB esté up.
