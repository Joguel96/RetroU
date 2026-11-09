# AGENTS.md — RetroU

Este archivo es la fuente de contexto para cualquier agente que trabaje en
RetroU. Léelo antes de proponer o tocar código.

## 1. Regla principal de cambios

**No modificar código, configuración, assets, documentación funcional ni
scripts sin autorización explícita del usuario.**

Antes de cualquier cambio hay que:

1. Explicar en 2–5 líneas:
   - qué archivo(s) se van a tocar;
   - qué comportamiento cambia;
   - qué riesgo tiene.
2. Esperar la autorización del usuario.
3. Solo entonces editar.
4. Compilar/verificar lo que se haya tocado.
5. Reportar qué se cambió y qué no se pudo probar.

Una petición del tipo “hazlo” autoriza el alcance descrito, no cambios
colaterales. No hacer refactors, limpiezas, renombrados ni “mejoras” no
pedidas. Si aparece un problema adicional, describirlo y pedir permiso antes
de arreglarlo.

La investigación de solo lectura (leer archivos, logs, documentación, ejecutar
builds de diagnóstico sin editar) no requiere autorización, salvo que el
usuario indique lo contrario.

## 2. Objetivo del proyecto

RetroU es un frontend propio para Nintendo Wii U (Aroma) para ejecutar juegos
retro, gestionar catálogo, carátulas, controles, guardados y ajustes.

RetroArch se usa **como referencia técnica**, no como base de código. No
reintroducir RetroArch en el árbol ni convertirlo en un fork.

Requisitos históricos del usuario:

- Interfaz con identidad RetroU, no la UI de RetroArch.
- Navegación solo con mando; teclado/ratón solo como fallback de desarrollo.
- Todo el ecosistema de datos bajo `retrou`, no `retroarch`.
- Un solo core cargado a la vez.
- Se prefieren soluciones simples y estables antes que arquitecturas
  experimentales.
- La app debe sentirse como RetroU, aunque por debajo cambie de RPX como
  RetroArch.

## 3. Arquitectura actual (`main`)

Modelo elegido: **apps RPX estáticas**, similar a RetroArch Wii U.

Cada app contiene:

```text
RetroU frontend + un core Libretro enlazado estáticamente
```

Apps:

| Target | Core | Sistema |
|---|---|---|
| `retrou_nes` | fceumm | NES |
| `retrou_snes` | snes9x | SNES |
| `retrou_gba` | mgba | GBA |
| `retrou_test` | `test_core.cpp` | test de vídeo |
| `retrou_launcher` | ninguno | launcher oficial |

Salidas:

```text
native/build/retrou_nes.rpx / .wuhb
native/build/retrou_snes.rpx / .wuhb
native/build/retrou_gba.rpx / .wuhb
native/build/retrou_test.rpx / .wuhb
native/build/retrou_launcher.rpx / .wuhb
```

Los RPX de core se copian a:

```text
dist/retrou/cores/RetroU-NES.rpx
dist/retrou/cores/RetroU-SNES.rpx
dist/retrou/cores/RetroU-GBA.rpx
dist/retrou/cores/RetroU-Test.rpx
```

El launcher se copia a:

```text
dist/wiiu/apps/retrou/RetroU.wuhb
dist/RetroU.wuhb
```

`dist/` está ignorado por git.

## 4. Estado del launcher

`RetroU.wuhb` es el ejecutable oficial: no tiene core; escanea ROMs y debería
lanzar el RPX correspondiente. Flujo previsto:

```text
RetroU.wuhb
  → A sobre ROM
  → escribe sd:/retrou/launch/last.txt
  → lanza sd:/retrou/cores/RetroU-<sistema>.rpx
  → el RPX lee last.txt, lo borra y carga la ROM
```

Antes del port HBL, el salto entre apps no funcionaba: al lanzar un juego el
sistema volvía al menú de Wii U. Se probaron rutas `fs:/vol`, `/vol` y `sd:/`
con `_SYSLaunchTitleByPathFromLauncher`; no era el método correcto.

El test `retrou_test.rpx` **sí funcionó**: era una app independiente, no un
salto entre apps. Ese éxito no valida el launcher.

### Port HBL implementado

El launcher ahora usa el mecanismo HBL de RetroArch en lugar de
`_SYSLaunchTitleByPathFromLauncher`:

- `native/src/hbl.hpp` / `hbl.cpp`
- `HBL_loadToMemory`
- `SC0x25_KernelCopyData` (syscall `0x2500`)
- mapa de memoria HBL (`MEM_BASE = 0x00800000`)
- `ARGV_MAGIC` / `ARGV_PTR` en `MEM_BASE + 0x1380`

El launcher escribe `launch/last.txt`, carga el RPX destino en HBL y sale. El
RPX lee `last.txt` y carga la ROM.

Pendiente: **probar en hardware**. Riesgos: `SC0x25` no permitido desde WUHB
de Aroma, conflicto con WUMSLoader en `0x00800000`, y dependencia de firmware.

## 5. Cores

Los cores no se bajan del buildbot como `.rpx`; esos `.rpx` son
RetroArch+core completos.

Nuestros cores se compilan desde fuente como `.a`:

```text
.cores/fceumm/fceumm_libretro_wiiu.a
.cores/snes9x/libretro/snes9x_libretro_wiiu.a
.cores/mgba/build/mgba_libretro.a
```

Script reproducible:

```text
native/cores/build-static-cores.sh
```

`.cores/` está ignorado por git. El script:

- clona fceumm, snes9x y mgba;
- compila fceumm y snes9x con `make platform=wiiu`;
- configura mgba con CMake y compila `mgba_libretro`;
- aplica el workaround `USE_FILE32API` en el minizip de mgba.

CMake añade fuentes de `libretro-common` necesarias para los cores estáticos:

- `compat_strl`
- `file_path`, `file_path_io`
- `file_stream`, `file_stream_transforms`
- `trans_stream`, `trans_stream_pipe`, `trans_stream_deflate`
- `memory_stream`
- `encoding_deflate`
- `stdstring`
- `vfs_implementation`

No añadir dependencias nuevas sin autorización.

## 6. RPL — rama experimental

El intento de cargar `.rpl` en proceso está en:

```text
experiment/rpl-loader
```

Estado:

- el loader descomprime secciones y aplica relocaciones;
- llegó a `OSCodegenCopy`, que falla;
- el userland de Wii U no permite ejecutar memoria normal;
- `OSDynLoad_Acquire` de Aroma no carga `.rpl` sueltos de la SD;
- el RPL no es la ruta principal y no debe meterse en `main`.

No extender el RPL sin petición explícita. Si se retoma, hacerlo en su rama.

## 7. UI y rendimiento

- UI de cristal propia, en español.
- Navegación con D-Pad/stick, A, B, X, Y, L/R, Minus.
- `Minus` abre el menú superpuesto de RetroU durante el core.
- El menú de juego pausa el core.
- El render de la UI es bajo demanda (`ui_dirty`), no a 60 FPS constantes.
- Contadores por consola y lista visible están cacheados.
- El usuario reportó que el catálogo seguía lento; la mejora de render bajo
  demanda está implementada pero **no validada en hardware**.
- Las fuentes Lato van embebidas en los RPX con `objcopy`; no dependen de
  `/vol/content`.
- Un RPX suelto no tiene icono ni splash; eso solo lo aporta el WUHB.

## 8. Logs

```text
sd:/retrou/retrou_rpx.log      app estática (init/run)
sd:/retrou/retrou_rpl.log      RPL (solo rama experimental)
sd:/retrou/launcher.log        intentos del launcher
sd:/retrou/launch/last.txt     ROM pendiente para el RPX destino
```

`last.txt` es de un solo uso: el RPX lo borra al arrancar.

## 9. Directorios y datos

```text
sd:/wiiu/apps/retrou/RetroU.wuhb
sd:/retrou/roms/<Consola>/
sd:/retrou/cores/RetroU-NES.rpx
sd:/retrou/cores/RetroU-SNES.rpx
sd:/retrou/cores/RetroU-GBA.rpx
sd:/retrou/cores/system/
sd:/retrou/cores/savefiles/
sd:/retrou/thumbnails/
sd:/retrou/launch/
```

No usar rutas `retroarch/`.

## 10. Build

Toolchain:

```text
C:/devkitPro
WUT
SDL2 / SDL2_ttf
CMake wrapper: /opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake
```

Build:

```sh
cd native
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

En Windows se usa MSYS2 bash para CMake/make.

No commitear:

- `native/build/`
- `.cores/`
- `dist/`
- logs
- artefactos `.rpx`, `.wuhb`, `.elf`, `.a`

## 11. Estado funcional y limitaciones

- NES/SNES/GBA: app RPX estática generada y compilada.
- `RetroU-Test` funciona en hardware (test de vídeo).
- No probado en hardware: NES/SNES/GBA con ROM real.
- **Input del core no implementado**: los botones aún no llegan al emulador.
- **Audio del core no implementado**.
- SRAM/savestates/carátulas reales pendientes.
- El launcher HBL está implementado; **no probado en hardware**.
- No afirmar que algo está probado en hardware si no lo está.

## 12. Convenciones

- Español para textos de usuario, logs de UI y documentación de producto.
- Identificadores de código en inglés.
- Cambios pequeños y localizados.
- No reformatear archivos completos.
- No cambiar la arquitectura sin autorización.
- Mantener `AGENTS.md`, `README.md` y `native/README.md` actualizados cuando
  cambie el comportamiento.
- Si una compilación falla, investigar y reportar; no ocultar errores.
- Si se requiere hardware, decirlo explícitamente y no inventar resultados.

## 13. Protocolo para cambios futuros

Antes de tocar código, presentar al usuario:

1. archivos que se crearán/modificarán;
2. explicación corta del cambio;
3. cómo se probará;
4. riesgos.

Esperar autorización. Después implementar en un cambio acotado y verificable.
