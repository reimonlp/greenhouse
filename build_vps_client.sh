#!/bin/bash
# Script para compilar y subir firmware VPS Client

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  Compilando ESP32 Greenhouse - VPS Client Mode              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Backup main.cpp
if [ -f "src/main.cpp" ]; then
    echo "📦 Backing up main.cpp..."
    cp src/main.cpp src/main.cpp.backup
fi

# Copy VPS client main
echo "📝 Using VPS client main..."
cp src/main_vps_client.cpp src/main.cpp

# Compile
echo "🔨 Compiling..."
pio run -e greenhouse-vps-client

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Compilation successful!"
    echo ""
    echo "📊 Memory usage:"
    pio run -e greenhouse-vps-client -t size
    echo ""
    
    read -p "¿Subir firmware al ESP32? (s/n): " -n 1 -r
    echo ""
    
    if [[ $REPLY =~ ^[Ss]$ ]]; then
        echo "📤 Uploading firmware..."
        pio run -t upload -e greenhouse-vps-client
        
        if [ $? -eq 0 ]; then
            echo ""
            echo "✅ Firmware uploaded successfully!"
            echo ""
            read -p "¿Abrir monitor serial? (s/n): " -n 1 -r
            echo ""
            
            if [[ $REPLY =~ ^[Ss]$ ]]; then
                pio device monitor -e greenhouse-vps-client
            fi
        else
            echo "❌ Upload failed!"
        fi
    fi
else
    echo "❌ Compilation failed!"
    
    # Restore backup
    if [ -f "src/main.cpp.backup" ]; then
        echo "🔄 Restoring main.cpp backup..."
        mv src/main.cpp.backup src/main.cpp
    fi
    
    exit 1
fi

# Clean up
if [ -f "src/main.cpp.backup" ]; then
    echo ""
    read -p "¿Restaurar main.cpp original? (s/n): " -n 1 -r
    echo ""
    
    if [[ $REPLY =~ ^[Ss]$ ]]; then
        mv src/main.cpp.backup src/main.cpp
        echo "✓ main.cpp restaurado"
    else
        rm src/main.cpp.backup
        echo "✓ Backup eliminado"
    fi
fi

echo ""
echo "✅ Done!"
