# RetroU — Agent Instructions

## Qué es este proyecto

Frontend ligero para Wii U (homebrew) que organiza y ejecuta ROMs de consolas retro (Atari, MSX, NES, SNES, Genesis, GBA, PS1, Arcade) invocando cores de Libretro. Objetivo: rápido, modular y visualmente atractivo con hardware limitado.

## Entorno y toolchain

- Toolchain: **devkitPro** + **WUT SDK** + entorno de ejecución **Aroma** (nada de APIs de HBL/cafiix salvo que se pida explícitamente).
- Estándar de lenguaje: **C++17**. Código modular, seguro en memoria y orientado al rendimiento (GPU/CPU de Wii U, RAM limitada).
- Variables de entorno obligatorias en Windows: `DEVKITPRO` (ej. `C:/devkitPro`) y `DEVKITPPC` deben existir o los builds fallan con errores confusos. Los includes de WUT/SDL quedan definidos por el Makefile de WUT, no por flags manuales.

## Reglas de código (obligatorias)

1. **Solo APIs reales de WUT**: `GX2` (render 2D y texturas, preferir VRAM para texturas), `VPAD` (GamePad) / `PAD` (Pro y otros mandos por USB), `coreinit` (hilos, FS). No inventar funciones: verificar contra los headers reales en `$DEVKITPRO/wut/include` y la documentación de la Libretro API antes de usar.
2. Gestionar memoria manualmente con cuidado; evitar patrones que allocen por frame.
3. Respetar la **Libretro API real**: los cores se cargan/llaman por su interfaz (`retro_init`, `retro_run`, callbacks de video/audio/input), no mediante wrappers inventados.
4. Respuestas: directas, técnicas y orientadas a la acción — código ejecutable + explicación breve de arquitectura.

## Estado del repo

- Repo actualmente vacío (solo este `AGENTS.md`). Cuando se creen Makefile/CMake y directorios, actualizar esta sección con los comandos reales verificados (build, flags del Makefile de WUT, cómo correr un solo target) en lugar de confiar en este texto.
