#!/bin/bash

# OTA Upload Script for ESP32 Greenhouse
# This script uploads firmware wirelessly to the ESP32

echo "╔══════════════════════════════════════════════╗"
echo "║  ESP32 Greenhouse - OTA Upload               ║"
echo "║  Over-The-Air Wireless Firmware Update       ║"
echo "╚══════════════════════════════════════════════╝"
echo ""

# Configuration
OTA_HOSTNAME="ESP32_GREENHOUSE_01.local"
OTA_IP=""  # Leave empty for auto-discovery, or set manual IP like "192.168.1.100"
OTA_PORT=3232
OTA_PASSWORD="greenhouse_ota_2024_secure"

# Build firmware first
echo "📦 Step 1: Building firmware..."
pio run
if [ $? -ne 0 ]; then
    echo "❌ Build failed!"
    exit 1
fi
echo "✅ Build successful"
echo ""

# Get firmware path
FIRMWARE_PATH=".pio/build/greenhouse-vps-client/firmware.bin"
if [ ! -f "$FIRMWARE_PATH" ]; then
    echo "❌ Firmware file not found: $FIRMWARE_PATH"
    exit 1
fi

FIRMWARE_SIZE=$(stat -f%z "$FIRMWARE_PATH" 2>/dev/null || stat -c%s "$FIRMWARE_PATH")
echo "📊 Firmware size: $(($FIRMWARE_SIZE / 1024)) KB"
echo ""

# Discover ESP32 IP if not set
if [ -z "$OTA_IP" ]; then
    echo "🔍 Step 2: Discovering ESP32..."
    echo "   Looking for: $OTA_HOSTNAME"
    
    # Try ping to resolve .local
    OTA_IP=$(ping -c 1 "$OTA_HOSTNAME" 2>/dev/null | grep -oE '([0-9]{1,3}\.){3}[0-9]{1,3}' | head -1)
    
    if [ -z "$OTA_IP" ]; then
        echo "⚠️  Auto-discovery failed. Please enter ESP32 IP manually:"
        read -p "   ESP32 IP address: " OTA_IP
    else
        echo "   ✅ Found ESP32 at: $OTA_IP"
    fi
else
    echo "🔍 Step 2: Using configured IP: $OTA_IP"
fi
echo ""

# Verify ESP32 is reachable
echo "🏓 Step 3: Checking connectivity..."
if ping -c 1 -W 2 "$OTA_IP" > /dev/null 2>&1; then
    echo "   ✅ ESP32 is reachable at $OTA_IP"
else
    echo "   ❌ Cannot reach ESP32 at $OTA_IP"
    echo "   Make sure:"
    echo "   1. ESP32 is powered on"
    echo "   2. Connected to WiFi (SSID: FDC)"
    echo "   3. You're on the same network"
    exit 1
fi
echo ""

# Upload via OTA
echo "📡 Step 4: Starting OTA upload..."
echo "   Target: $OTA_IP:$OTA_PORT"
echo "   This will take about 30 seconds..."
echo ""

# Use espota.py for OTA upload
python3 ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py \
    -i "$OTA_IP" \
    -p "$OTA_PORT" \
    -f "$FIRMWARE_PATH" \
    -a "$OTA_PASSWORD" \
    -d

if [ $? -eq 0 ]; then
    echo ""
    echo "╔══════════════════════════════════════════════╗"
    echo "║  ✅ OTA Upload Successful!                   ║"
    echo "╚══════════════════════════════════════════════╝"
    echo ""
    echo "The ESP32 is now rebooting with new firmware."
    echo "Monitor output with: pio device monitor"
else
    echo ""
    echo "╔══════════════════════════════════════════════╗"
    echo "║  ❌ OTA Upload Failed                        ║"
    echo "╚══════════════════════════════════════════════╝"
    echo ""
    echo "Common issues:"
    echo "1. Wrong password (check secrets.h)"
    echo "2. ESP32 not reachable on network"
    echo "3. Firewall blocking port $OTA_PORT"
    echo "4. ESP32 busy or crashed"
    echo ""
    echo "Try USB upload instead: pio run --target upload"
    exit 1
fi
