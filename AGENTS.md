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
5. **Ciclo de vida ProcUI obligatorio** (bug de pantalla negra verificado en hardware): toda app WUT necesita `ProcUIInitEx()` + loop con `ProcUIProcessMessages()` o el OS **no otorga foreground** → pantallas negras y GamePad sin respuesta (ni HOME). Los framebuffers van del **MEM1 frame-heap** (`MEMAllocFromFrmHeapEx` con tag + `MEMRecordStateForFrmHeap`/`MEMFreeByStateToFrmHeap`), NO de `OSAllocFromSystem` (TV pide ~7.4MB y excede el system heap). Registro de callbacks `PROCUI_CALLBACK_ACQUIRE/RELEASE` + `ProcUIDrawDoneRelease()` en `PROCUI_STATUS_RELEASE_FOREGROUND`, según `libwhb/console.c` y `libwhb/proc.c` (referencias canónicas en `$DEVKITPRO/wut` y su repo).

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

- **Formato ejecutable vs paquete**: `.rpx` sigue siendo el binario nativo que el OS carga; `.wuhb` es el formato de homebrew del **menu de Aroma** (icono + splashes TV/DRC TGA + binario dentro). Aroma NO lista `.rpx` sueltos en su menu de homebrew: distribuir como `.wuhb` (o `.rpx` bajo estructura de directorios con icono/meta para SD clásica). `wut_create_wuhb` exige haber llamado antes a `wut_create_rpx` (lee la propiedad `WUT_RPL`/`WUT_IS_RPX` del target).
- Assets WUHB en `assets/` (`icon.png` 128x128, `tv-splash.png` 1280x720, `drc-splash.png` 854x480).
- **Script de build + staging**: `scripts/build.sh` (o `build.cmd` en Windows, doble clic). Compila (`-jN` auto detectado, `JOBS=N` para fijar, `--fresh` para rebuild completo), verifica `.elf/.rpx/.wuhb` y arma `dist/` listo para la raíz de la SD: `wiiu/apps/RetroU/RetroU.wuhb` + `retroU/roms/<Consola>/` (Atari, MSX, NES, SNES, Genesis, GBA, PS1, Arcade — convención de rutas que usará el escaneo del frontend) + `retroU/cores/` (fase Libretro). Rebuild incremental sin configure cada vez que cambia CMake: ejecutar `cmake --build build` directo es válido para iterar rápido.
- `powerpc-eabi-cmake` define `CMAKE_SYSTEM_NAME=CafeOS` automáticamente; **NO** usar `find_package(wut)` ni toolchain files viejos. `libwut.a` (WUT ≥1.9) ya contiene los import stubs de TODA la API Cafe (OSScreen*, GX2*, VPAD*, SYSLaunchMenu... verificado con `powerpc-eabi-nm`) — no existen `libgx2.a`, etc., ni hace falta enlazarlas: el toolchain enlaza `-lwut -lm` solo. Fuentes reales en Windows: `C:/devkitPro/{wut,cmake,tools,msys2}`; dentro de msys2 bash, `/opt/devkitpro` mapea a `C:/devkitPro`.
- Fase 1 del renderer usa **OSScreen** (`coreinit/screen.h`), no GX2 directo: el render GX2 nativo exige shaders compilados offline (cafeglsl/deflect) y aún no hay pipeline de shaders. Datos verificados: formato de pixel `0xRRGGBBAA` (bytes R,G,B,A en memoria, big-endian), TV 1280x720 (stride 1280), DRC 854x480 (stride 896 visibles 854), buffers primario+secundario por pantalla, alineación 0x100 en `OSScreenSetBufferEx`, `DCFlushRange` antes de `OSScreenFlipBuffersEx`.
- `VPADInit`/`VPADShutdown` están `WUT_DEPRECATED`: leer `VPADRead` directo. Excepción: si `VPADRead` reporta `VPAD_READ_UNINITIALIZED`, el mismo header indica llamar `VPADInit()` (fallback único en `main.cpp` con pragma `-Wdeprecated-declarations`).
- Salida: el ciclo ProcUI manda. HOME abre el overlay del OS (comportamiento estándar de app); en `PROCUI_STATUS_EXITING` → `renderer.shutdown()` + `ProcUIShutdown()` + `return 0` (sin `SYSLaunchMenu`: el OS decide el destino). `SYSLaunchMenu()` solo se usaría para salidas iniciadas por la app (ya no aplica en el flujo actual).
- **devkitPro YA está instalado en Windows**: `C:/devkitPro` (devkitPPC GCC 16.2.0, wut 1.9.2, wut-tools 1.3.5, wiiu-cmake 1.3.2). Variables globales quedan en formato POSIX (`/opt/devkitpro`): útiles dentro de msys2; en PowerShell/cmd usar rutas `C:/devkitPro`. `scripts/build.sh`/`build.cmd` auto-detectan ambas.
