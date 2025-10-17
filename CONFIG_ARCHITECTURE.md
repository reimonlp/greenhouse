# 🏗️ Configuration Architecture Guide

**Status:** ✅ Cleaned & Audited  
**Date:** January 16, 2025  
**Version:** 2.0

## Overview

This document defines the **single source of truth** principle for all configuration in the ESP32 Greenhouse project. After a comprehensive audit, we've eliminated duplicate credential definitions and established a clear hierarchy.

## Problem History

### What Was Wrong (Before v2.3)

**Issue:** Multiple files defined the same credentials with conflicting values:

```cpp
// config.h (included FIRST in main.cpp line 8)
#define WIFI_SSID "FDC"
#define WIFI_PASSWORD "unacagada"
#define API_TOKEN "tu_token_secreto_aqui"
#define OTA_PASSWORD "ota_password_123"

// secrets.h (included LATER in main.cpp line 15)
#ifndef WIFI_SSID
#define WIFI_SSID "FDC"  // ❌ NEVER EXECUTED due to #ifndef
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD "greenhouse_ota_2024_secure"  // ❌ NEVER EXECUTED
#endif
```

**Impact:**
- OTA authentication failed because ESP32 used `config.h` value, not `secrets.h`
- Security risk: developers thought they were changing passwords in `secrets.h`, but values from `config.h` were actually used
- Confusion about which file controlled credentials

**Root Cause:** Include order precedence + `#ifndef` guards meant `secrets.h` could never override `config.h`.

### Discovery

Audit revealed **7 duplicate credential definitions**:
```
WIFI_SSID:              3 occurrences (config.h, secrets.h, secrets.example.h)
WIFI_PASSWORD:          3 occurrences (config.h, secrets.h, secrets.example.h)
API_TOKEN:              3 occurrences (config.h, secrets.h, secrets.example.h)
OTA_PASSWORD:           2 occurrences (config.h, secrets.h)
MONGODB_URI:            2 occurrences (secrets.h, secrets.example.h)
MONGODB_DATA_API_KEY:   2 occurrences (secrets.h, secrets.example.h)
DEVICE_AUTH_TOKEN:      2 occurrences (secrets.h, secrets.example.h)
```

## Current Architecture (v2.3+)

### File Hierarchy

```
┌─────────────────────────────────────────────────────────────┐
│ 1. secrets.h (SINGLE SOURCE OF TRUTH)                      │
│    - All sensitive credentials                              │
│    - WiFi, API tokens, OTA password, MongoDB credentials    │
│    - Uses #ifndef guards for safety                         │
│    - NOT committed to git (.gitignore)                      │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. secrets.example.h (TEMPLATE for users)                  │
│    - Template with placeholder values                       │
│    - Safe to commit to git                                  │
│    - Users copy this to secrets.h and fill real values     │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. config.h (NON-SENSITIVE configuration)                  │
│    - Timeouts, limits, feature flags                        │
│    - Log levels, debug settings                             │
│    - Hardware mappings (via pins.h)                         │
│    - ❌ NO CREDENTIALS allowed here                         │
└─────────────────────────────────────────────────────────────┘
```

### Include Order in main.cpp

```cpp
#include <Arduino.h>
#include <WiFi.h>
// ... other Arduino libraries ...

#include "config.h"        // Line 8:  Non-sensitive config
#include "vps_config.h"    // Line 9:  Device identity
#include "vps_client.h"    // Line 10: Client implementation
#include "vps_websocket.h" // Line 11: WebSocket implementation
#include "vps_ota.h"       // Line 12: OTA configuration
#include "sensors.h"       // Line 13: Sensor definitions
#include "relays.h"        // Line 14: Relay definitions
#include "secrets.h"       // Line 15: 🔑 CREDENTIALS (last, overrides all)
```

**Critical:** `secrets.h` is included **LAST** so its values take precedence.

## Configuration Files

### 1. secrets.h (Credentials - NOT in git)

**Purpose:** Single source of truth for all sensitive credentials

**Location:** `include/secrets.h`

**Git Status:** ❌ Ignored (in `.gitignore`)

**What Goes Here:**
```cpp
#ifndef SECRETS_H
#define SECRETS_H

// WiFi credentials
#ifndef WIFI_SSID
#define WIFI_SSID "YourNetworkName"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YourWiFiPassword"
#endif

// API authentication
#ifndef API_TOKEN
#define API_TOKEN "your_256bit_hex_token_here"
#endif

// Device authentication for backend
#ifndef DEVICE_AUTH_TOKEN
#define DEVICE_AUTH_TOKEN "your_device_token_here"
#endif

// MongoDB Atlas credentials
#ifndef MONGODB_URI
#define MONGODB_URI "mongodb+srv://user:pass@cluster.mongodb.net/..."
#endif

// OTA update password
#ifndef OTA_PASSWORD
#define OTA_PASSWORD "your_secure_ota_password"
#endif

#endif
```

**Rules:**
- ✅ All values use `#ifndef` guards
- ✅ No values are commented out
- ✅ Production values only
- ❌ Never commit to git
- ❌ Never hardcode test values

### 2. secrets.example.h (Template - IN git)

**Purpose:** Template for users to create their own `secrets.h`

**Location:** `include/secrets.example.h`

**Git Status:** ✅ Committed (safe template)

**What Goes Here:**
```cpp
#ifndef SECRETS_H
#define SECRETS_H

// WiFi credentials - REPLACE THESE
#ifndef WIFI_SSID
#define WIFI_SSID "YourSSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YourPasswordHere"
#endif

// API Token - GENERATE A SECURE RANDOM TOKEN
#ifndef API_TOKEN
#define API_TOKEN "REPLACE_WITH_256BIT_HEX_TOKEN"
#endif

// ... more placeholder examples ...

#endif
```

**Rules:**
- ✅ Placeholder values only
- ✅ Clear instructions in comments
- ✅ Safe to commit
- ✅ Keep in sync with secrets.h structure
- ❌ No real credentials

### 3. config.h (System Settings - IN git)

**Purpose:** Non-sensitive system configuration

**Location:** `include/config.h`

**Git Status:** ✅ Committed

**What Goes Here:**
```cpp
#ifndef CONFIG_H
#define CONFIG_H

// Timeouts and limits
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_RETRY_BASE_MS      5000
#define MAX_API_REQUESTS        100

// Safety limits
#define MAX_TEMP_CELSIUS        35.0f
#define MIN_TEMP_CELSIUS        5.0f
#define MAX_HUMIDITY_PERCENT    95.0f

// Feature flags
#define WATCHDOG_TIMEOUT_SEC    120
#define LOG_LEVEL               4  // 0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG

// NTP server
#define NTP_SERVER          "pool.ntp.org"
#define GMT_OFFSET_SEC      -10800

#endif
```

**Rules:**
- ✅ Non-sensitive values only
- ✅ Timeouts, limits, thresholds
- ✅ Feature flags and log levels
- ✅ Hardware pin mappings (via pins.h)
- ❌ NO credentials or secrets
- ❌ NO WiFi passwords
- ❌ NO API tokens
- ❌ NO MongoDB URIs

## Setup Instructions for New Developers

### First Time Setup

1. **Clone the repository**
   ```bash
   git clone https://github.com/reimonlp/greenhouse.git
   cd greenhouse
   ```

2. **Create your secrets file**
   ```bash
   cp include/secrets.example.h include/secrets.h
   ```

3. **Edit secrets.h with real values**
   ```bash
   nano include/secrets.h
   ```
   
   Fill in:
   - Your WiFi SSID and password
   - Generate a secure API token (256-bit hex)
   - Set your MongoDB credentials
   - Create a secure OTA password

4. **Verify secrets.h is ignored**
   ```bash
   git status
   # secrets.h should NOT appear in untracked files
   ```

5. **Build and upload**
   ```bash
   pio run --target upload
   ```

### Verifying Configuration

#### Check which values are being used:

```bash
# Search for credentials in compiled binary
strings .pio/build/greenhouse-vps-client/firmware.elf | grep -i "greenhouse_ota"

# Check preprocessor output (shows final values after all #defines)
pio run -t preprocess > /tmp/preprocessed.cpp
grep "WIFI_SSID\|API_TOKEN\|OTA_PASSWORD" /tmp/preprocessed.cpp
```

#### Test OTA with current password:

```bash
# Extract password from secrets.h
grep "OTA_PASSWORD" include/secrets.h

# Test OTA upload
python3 ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py \
    -i <ESP32_IP> \
    -p 3232 \
    -a "<password_from_secrets.h>" \
    -f .pio/build/greenhouse-vps-client/firmware.bin
```

## Security Best Practices

### 1. Credential Management

- ✅ **DO** use `secrets.h` for all credentials
- ✅ **DO** use strong, unique passwords
- ✅ **DO** use 256-bit hex tokens for API authentication
- ✅ **DO** rotate credentials periodically
- ❌ **DON'T** commit `secrets.h` to git
- ❌ **DON'T** hardcode credentials in `config.h`
- ❌ **DON'T** share credentials in documentation
- ❌ **DON'T** use weak passwords like "123456"

### 2. OTA Security

- ✅ **DO** use a strong OTA password (20+ characters)
- ✅ **DO** change default OTA password immediately
- ✅ **DO** disable OTA in production if not needed
- ❌ **DON'T** use simple passwords like "ota_password_123"
- ❌ **DON'T** expose OTA port to public internet

### 3. WiFi Security

- ✅ **DO** use WPA2/WPA3 encryption
- ✅ **DO** use a guest network for IoT devices
- ✅ **DO** firewall ESP32 from other network devices
- ❌ **DON'T** use open WiFi networks
- ❌ **DON'T** expose ESP32 management ports publicly

### 4. API Tokens

- ✅ **DO** generate cryptographically random tokens
- ✅ **DO** use at least 256 bits of entropy
- ✅ **DO** implement rate limiting (already done)
- ❌ **DON'T** use predictable tokens
- ❌ **DON'T** reuse tokens across devices

## Migration Guide

### Upgrading from Pre-v2.3

If you have an existing installation with credentials in `config.h`:

1. **Backup current credentials**
   ```bash
   grep "WIFI_SSID\|WIFI_PASSWORD\|API_TOKEN\|OTA_PASSWORD" include/config.h > /tmp/old_creds.txt
   ```

2. **Pull latest changes**
   ```bash
   git pull origin main
   ```

3. **Update secrets.h**
   ```bash
   nano include/secrets.h
   # Copy your old credentials from /tmp/old_creds.txt
   ```

4. **Verify config.h is clean**
   ```bash
   grep "WIFI_SSID\|WIFI_PASSWORD\|API_TOKEN\|OTA_PASSWORD" include/config.h
   # Should show only comments, no actual definitions
   ```

5. **Upload new firmware**
   ```bash
   # First upload via USB (ESP32 still has old OTA password)
   pio run --target upload
   
   # Wait for reboot, then verify new password works
   python3 espota.py -i <ESP32_IP> -a "<new_password_from_secrets.h>" -f firmware.bin
   ```

## Troubleshooting

### OTA Authentication Fails

**Symptom:** `espota.py` returns "Authentication Failed"

**Possible Causes:**
1. Using wrong password (check `secrets.h`)
2. ESP32 still running old firmware with old password
3. Typo in password (check for spaces, quotes)

**Solution:**
```bash
# 1. Verify password in secrets.h
grep "OTA_PASSWORD" include/secrets.h

# 2. Check if ESP32 was updated (via serial monitor)
pio device monitor --raw | grep "Firmware"

# 3. If still on old firmware, upload via USB first
pio run --target upload

# 4. Try OTA with new password
python3 espota.py -i <IP> -a "<password_from_secrets.h>" -f firmware.bin
```

### WiFi Connection Fails

**Symptom:** ESP32 can't connect to WiFi

**Check:**
```bash
# 1. Verify SSID and password in secrets.h
grep "WIFI_" include/secrets.h

# 2. Check if config.h still has WiFi credentials (it shouldn't)
grep "WIFI_SSID\|WIFI_PASSWORD" include/config.h

# 3. Monitor serial output during boot
pio device monitor --raw | grep -i "wifi\|connect"
```

### Build Fails with "Undefined Reference"

**Symptom:** Linker errors about missing definitions

**Possible Cause:** Missing credentials in `secrets.h`

**Solution:**
```bash
# 1. Check secrets.h exists
ls -la include/secrets.h

# 2. If missing, copy from template
cp include/secrets.example.h include/secrets.h

# 3. Fill in all required values
nano include/secrets.h

# 4. Rebuild
pio run --target clean
pio run
```

## Audit Results

### Configuration Cleanup (v2.3)

✅ **Completed:**
- Removed `WIFI_SSID` from `config.h` (now only in `secrets.h`)
- Removed `WIFI_PASSWORD` from `config.h` (now only in `secrets.h`)
- Removed `API_TOKEN` from `config.h` (now only in `secrets.h`)
- Removed `OTA_PASSWORD` from `config.h` (now only in `secrets.h`)
- Verified all credentials use `#ifndef` guards
- Tested OTA with new password: ✅ SUCCESS
- Tested WiFi connection: ✅ SUCCESS
- Tested API authentication: ✅ SUCCESS

### Remaining Issues

⚠️ **Minor:** `FIRMWARE_VERSION` is defined in both `config.h` and `vps_config.h` (causes harmless warning)

**Impact:** Low - just a compiler warning, `vps_config.h` value wins

**Fix:** Remove `FIRMWARE_VERSION` from `config.h` in future cleanup

## Architecture Decisions

### Why secrets.h Last in Include Order?

**Reason:** With `#ifndef` guards, the FIRST definition wins. By including `secrets.h` last, we ensure:
1. If someone accidentally defines a credential in another file, `secrets.h` doesn't override it (shows the mistake)
2. Developers can see the full context when editing `secrets.h`
3. Clear separation: system config → device config → credentials

### Why Use #ifndef Guards?

**Benefits:**
- Prevents accidental redefinition errors
- Allows conditional compilation for testing
- Compatible with `-D` compiler flags
- Safe fallback to template values if secrets.h missing

**Alternative Considered:** Remove guards, let compiler error if missing
- **Rejected:** Too fragile for new developers

### Why Keep secrets.example.h?

**Benefits:**
- New developers can see what credentials are needed
- Documents expected format and length
- Safe to commit (no real credentials)
- CI/CD can use example values for build testing

## Future Improvements

### Planned for v2.4+

- [ ] Add script to validate `secrets.h` completeness
- [ ] Add compile-time check for placeholder values
- [ ] Add encrypted credential storage option
- [ ] Add credential rotation automation
- [ ] Remove `FIRMWARE_VERSION` duplicate from `config.h`

### Under Consideration

- [ ] Use ESP32 secure storage for OTA password
- [ ] Implement certificate-based authentication
- [ ] Add remote credential provisioning via BLE
- [ ] Use hardware-backed encryption keys

## Summary

**Key Principles:**
1. 🔑 **`secrets.h` is the single source of truth for credentials**
2. 📝 **`config.h` contains non-sensitive settings only**
3. 📄 **`secrets.example.h` is a safe template**
4. 🚫 **Never commit real credentials to git**
5. ✅ **Always use `#ifndef` guards**

**Results:**
- ✅ OTA authentication working with correct password
- ✅ All credential conflicts resolved
- ✅ Clear hierarchy established
- ✅ Security improved
- ✅ Documentation complete

---

**Status:** 🎯 Architecture Clean  
**Security:** ✅ Credentials Protected  
**Documentation:** ✅ Complete  
**Testing:** ✅ Verified
