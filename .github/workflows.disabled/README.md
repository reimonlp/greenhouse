# Workflows Deshabilitados

Estos workflows de GitHub Actions han sido deshabilitados temporalmente.

## Archivos:

- **ci.yml** - Compilación automática de firmware ESP32
- **clang-tidy.yml** - Análisis estático de código C++

## Estado actual:

✅ **Actualizados** para usar los environments correctos:
- `greenhouse` (firmware local)
- `greenhouse-vps-client` (firmware VPS actual)

## Para reactivarlos:

```bash
# Renombrar la carpeta de vuelta
mv .github/workflows.disabled .github/workflows

# Los workflows se ejecutarán en el próximo push
git add .github/workflows/
git commit -m "Enable GitHub Actions workflows"
git push
```

## Notas:

- Los workflows fueron configurados originalmente para environments que ya no existen
- Han sido actualizados pero deshabilitados por decisión del usuario
- Si necesitas CI/CD en el futuro, estos workflows están listos para usar

---
*Deshabilitado el: 2025-10-14*
*Razón: No se están utilizando GitHub Actions actualmente*
