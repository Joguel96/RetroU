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

- Estructura inicial creada: `CMakeLists.txt`, `src/main.cpp`, `src/gfx/renderer.{hpp,cpp}`, `src/ui/menu.{hpp,cpp}`, `src/core/launcher.{hpp,cpp}`.
- **Build (verificado contra toolchain devkitPro CMake moderno, wut.toolchain.cmake está deprecado)**:

  ```bash
  # requiere devkitPPC, wut, wut-tools, wiiu-cmake en $DEVKITPRO
  $DEVKITPRO/portlibs/wiiu/bin/powerpc-eabi-cmake -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j
  # salida: build/retroU.rpx (via wut_create_rpx -> elf2rpl POST_BUILD)
  # salida: build/retroU.wuhb (via wut_create_wuhb -> wuhbtool, para Aroma)
  ```

- **Formato ejecutable vs paquete**: `.rpx` sigue siendo el binario nativo que el OS carga; `.wuhb` es el formato de homebrew del **menu de Aroma** (icono + splashes TV/DRC TGA + binario dentro). Aroma NO lista `.rpx` sueltos en su menu de homebrew: distribuir como `.wuhb` (o `.rpx` bajo estructura de directorios con icono/meta para SD clásica). `wut_create_wuhb` exige haber llamado antes a `wut_create_rpx` (fade: lee la propiedad `WUT_RPL`/`WUT_IS_RPX` del target).
- Assets WUHB en `assets/` (`icon.png` 128x128, `tv-splash.png` 1280x720, `drc-splash.png` 854x480).
- `powerpc-eabi-cmake` define `CMAKE_SYSTEM_NAME=CafeOS` automáticamente; **NO** usar `find_package(wut)` ni toolchain files viejos. `libwut.a` (WUT ≥1.9) ya contiene los import stubs de TODA la API Cafe (OSScreen*, GX2*, VPAD*, SYSLaunchMenu... verificado con `powerpc-eabi-nm`) — no existen `libgx2.a`, etc., ni hace falta enlazarlas: el toolchain enlaza `-lwut -lm` solo. Fuentes reales en Windows: `C:/devkitPro/{wut,cmake,tools,msys2}`; dentro de msys2 bash, `/opt/devkitpro` mapea a `C:/devkitPro`.
- Fase 1 del renderer usa **OSScreen** (`coreinit/screen.h`), no GX2 directo: el render GX2 nativo exige shaders compilados offline (cafeglsl/deflect) y aún no hay pipeline de shaders. Datos verificados: formato de pixel `0xRRGGBBAA` (bytes R,G,B,A en memoria, big-endian), TV 1280x720 (stride 1280), DRC 854x480 (stride 896 visibles 854), buffers primario+secundario por pantalla, alineación 0x100 en `OSScreenSetBufferEx`, `DCFlushRange` antes de `OSScreenFlipBuffersEx`.
- `VPADInit`/`VPADShutdown` están `WUT_DEPRECATED`: leer `VPADRead` directo.
- Salida limpia: `SYSLaunchMenu()` (sysapp/launch.h) después de `renderer.shutdown()`.
- Pendiente instalar devkitPro localmente (no existe `C:/devkitPro` en esta máquina; hasta entonces el build solo es posible en CI/docker con la imagen devkitPro).
