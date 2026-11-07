#!/usr/bin/env bash
# =============================================================================
# build.sh - Compila RetroU y arma la estructura lista para la SD (Aroma)
# =============================================================================
#
# Uso (desde la raiz del repo):
#   ./scripts/build.sh                 # build incremental + staging dist/
#   ./scripts/build.sh --fresh         # borra build/ y dist/ antes de compilar
#   JOBS=8 ./scripts/build.sh          # fija nro de hilos de compilacion
#
# Salida:
#   build/retroU.rpx  - binario nativo Wii U (wut_create_rpx/elf2rpl)
#   build/retroU.wuhb - paquete homebrew de Aroma (wuhbtool)
#   dist/             - estructura lista para copiar a la raiz de la SD:
#       wiiu/apps/RetroU/RetroU.wuhb      -> homebrew de Aroma
#       retroU/roms/<Consola>/            -> ROMs que escaneara el frontend
#       retroU/cores/                     -> cores Libretro (fase 2/3)
#
# Requisitos: devkitPro con devkitPPC, wut, wut-tools, wiiu-cmake.
#   - Las variables DEVKITPRO/DEVKITPPC se establecen automaticamente si
#     faltan (msys2 de Windows las trae ya configuradas).

set -euo pipefail

cd "$(dirname "$0")/.."   # raiz del repo

# --- Localizar devkitPro -----------------------------------------------------
if [ -z "${DEVKITPRO:-}" ]; then
    if [ -d /opt/devkitpro ]; then
        export DEVKITPRO=/opt/devkitpro
    elif [ -d "C:/devkitPro" ]; then
        export DEVKITPRO="C:/devkitPro"
    else
        echo "ERROR: devkitPro no encontrado." >&2
        echo "Instale devkitPPC + wut + wut-tools + wiiu-cmake (ver AGENTS.md)." >&2
        exit 1
    fi
fi
: "${DEVKITPPC:=${DEVKITPRO}/devkitPPC}"
export DEVKITPPC

CMAKE_WRAPPER="${DEVKITPRO}/portlibs/wiiu/bin/powerpc-eabi-cmake"
if [ ! -x "$CMAKE_WRAPPER" ]; then
    echo "ERROR: no existe $CMAKE_WRAPPER" >&2
    echo "Falta el paquete wiiu-cmake: pacman -S wiiu-cmake" >&2
    exit 1
fi

# --- Opciones ------------------------------------------------------------------
FRESH=0
for arg in "$@"; do
    case "$arg" in
        --fresh) FRESH=1 ;;
        *) echo "opcion desconocida: $arg" >&2; exit 1 ;;
    esac
done

if [ "$FRESH" = 1 ]; then
    echo ">> limpiando build/ y dist/ (--fresh)"
    rm -rf build dist
fi

# --- Build -------------------------------------------------------------------
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

echo ">> configurando (devkitPro CMake, toolchain CafeOS)"
"$CMAKE_WRAPPER" -B build -DCMAKE_BUILD_TYPE=Release

echo ">> compilando (-j$JOBS)"
cmake --build build -j"$JOBS"

# --- Verificar artefactos -----------------------------------------------------
for art in build/retroU.elf build/retroU.rpx build/retroU.wuhb; do
    if [ ! -f "$art" ]; then
        echo "ERROR: no se genero $art" >&2
        exit 1
    fi
done
echo ">> artefactos OK: retroU.elf / retroU.rpx / retroU.wuhb"

# --- Incrementar version en dist (staging) ------------------------------------
# Estructura final en la SD (convenciones de RetroU):
#   wiiu/apps/RetroU/RetroU.wuhb   -> asi lo lista el menu homebrew de Aroma
#   retroU/roms/<Consola>/         -> rutas que escaneara el frontend (fase 2)
#   retroU/cores/                  -> carga de cores Libretro .rpl (fase 3)
DIST=dist
rm -rf "$DIST"
mkdir -p "$DIST/wiiu/apps/RetroU"
mkdir -p "$DIST/retroU/cores"

cp build/retroU.wuhb "$DIST/wiiu/apps/RetroU/RetroU.wuhb"

# Una carpeta de ROMs por consola objetivo (orden del menu de RetroU)
for console in Atari MSX NES SNES Genesis GBA PS1 Arcade; do
    mkdir -p "$DIST/retroU/roms/$console"
done

cat > "$DIST/README.txt" <<'EOF'
RetroU - estructura para la tarjeta SD
======================================

Copie el CONTENIDO de esta carpeta a la raiz de la tarjeta SD de la Wii U:

  wiiu/apps/RetroU/RetroU.wuhb   RetroU en el menu homebrew de Aroma.
  retroU/roms/<Consola>/         Coloque aqui sus ROMs, una carpeta por consola
                                 (Atari, MSX, NES, SNES, Genesis, GBA, PS1,
                                 Arcade).
  retroU/cores/                  Reservado para los cores Libretro (fase 2/3
                                 del proyecto; colocar ahi los .rpl cargables).

Nota: los cores todavia no estan implementados; RetroU arrancara y mostrara
el menu, pero "A: cargar" no ejecutara ROMs hasta la fase Libretro.
EOF

echo ">> dist/ listo:"
find "$DIST" -type d | sort | sed 's/^/     /'
echo
echo "=== RETROU BUILD COMPLETO ==="
