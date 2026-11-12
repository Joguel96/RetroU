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

Modelo elegido: **una WUHB independiente por consola**, como las apps virtuales
de Switch. No hay launcher ni carpeta de cores en la SD.

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

Salidas:

```text
native/build/retrou_nes.wuhb
native/build/retrou_snes.wuhb
native/build/retrou_gba.wuhb
```

Distribución:

```text
dist/wiiu/apps/RetroNESU/RetroNESU.wuhb
dist/wiiu/apps/RetroSNESU/RetroSNESU.wuhb
dist/wiiu/apps/RetroGBAU/RetroGBAU.wuhb
```

No se distribuyen RPX sueltos ni `RetroU.wuhb` launcher.

## 4. Datos y configuración

```text
sd:/retrou/roms/nes/
sd:/retrou/roms/snes/
sd:/retrou/roms/gba/

sd:/retrou/config/theme.conf              tema compartido

sd:/retrou/config/nes/
  nes.conf
  control-map.conf
  video.conf
  view.conf
sd:/retrou/config/snes/
  snes.conf
  control-map.conf
  video.conf
  view.conf
sd:/retrou/config/gba/
  gba.conf
  control-map.conf
  video.conf
  view.conf

sd:/retrou/saves/<consola>/save/          SRAM de juegos
sd:/retrou/snap/<consola>/                slots de guardado de la app
```

Las carpetas de ROMs se aceptan en minúscula (`nes`, `snes`, `gba`) y también
en mayúscula por compatibilidad. La ruta canónica es minúscula.

`theme.conf` es común. `<consola>.conf`, `control-map.conf`, `video.conf` y
`view.conf` son independientes por app.

`control-map.conf` usa el formato `v2` estilo Our64 (acción del core → una o
varias máscaras de botones físicos): secciones `[gamepad]` y `[pro]`, líneas
`p1`/`p2` y una línea por acción. Las acciones sin asignación se escriben como
`none`, para que `X Unmap` sobreviva al reinicio. Los conf en formato `v1` o
viejo (una línea de 13 ints) se migran al cargar y se reescriben al primer
cambio.

Ya no se usa `sd:/retrou/cores`, HBL, Mocha ni `launch/last.txt`.

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

- UI de cristal propia, en español, guiada por `Joguel96/retroui`.
- Navegación con D-Pad/stick, A, B, Plus.
- No hay barra lateral de consolas; `Plus` abre Ajustes en dos paneles.
- La biblioteca ofrece cards, lista y carrusel; lista/carrusel tienen filtro
  alfabético con L/R. La vista y el fondo dinámico se guardan en `view.conf`.
- `Minus` abre el sidebar de filtros solo en cards; en lista/carrusel abre
  el buscador. Las cards permiten entre 3 y 6 columnas.
- `Minus + Plus` abre el menú superpuesto durante el core.
- El menú de juego pausa el core.
- El render de la UI es bajo demanda (`ui_dirty`), no a 60 FPS constantes.
- Las fuentes Lato van embebidas en los WUHB con `objcopy`.

## 8. Logs

```text
sd:/retrou/retrou_rpx.log      app estática (init/run; .1/.2/.3 anteriores)
sd:/retrou/retrou_rpl.log      RPL (solo rama experimental)
```

Ya no se usa `launcher.log` ni `launch/last.txt`.

## 9. Directorios y datos

```text
sd:/wiiu/apps/RetroNESU/RetroNESU.wuhb
sd:/wiiu/apps/RetroSNESU/RetroSNESU.wuhb
sd:/wiiu/apps/RetroGBAU/RetroGBAU.wuhb

sd:/retrou/roms/nes/
sd:/retrou/roms/snes/
sd:/retrou/roms/gba/

sd:/retrou/config/theme.conf
sd:/retrou/config/<consola>/<consola>.conf
sd:/retrou/config/<consola>/control-map.conf
sd:/retrou/config/<consola>/video.conf
sd:/retrou/config/<consola>/view.conf

sd:/retrou/saves/<consola>/save/
sd:/retrou/snap/<consola>/
```

No usar rutas `retroarch/` ni `retrou/cores/`.

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

**La lista canónica de pendientes es `PENDIENTES.md`** (raíz del proyecto):
antes de empezar una actividad hay que revisarlo, y cada pendiente terminado
**y verificado en hardware** se marca como ✅ allí (con fecha). Este resumen
solo mantiene el estado general.

- NES/SNES/GBA: WUHB estática generada y compilada.
- N64: `RetroN64U.wuhb` generada desde Our64 (Not64 + GLideN64, clon externo
  en `C:/proyectos/Our64`), parcheada para WUT 1.9.2 y rebrandeada como
  RetroU N64 con la UI propia de Our64. No probado en hardware. Pendiente el
  port con la UI completa de RetroU (varias rondas, ver `PENDIENTES.md`).
- **Probado en hardware por el usuario**: HOME durante partida, audio del
  core, NES/SNES/GBA con ROMs reales, savestates y carátulas, conexión de
  Pro Controller.
- **Input del core implementado**: A/B/X/Y/L/R, Start/Select y D-pad/stick.
- **Input UI**: GamePad (VPAD) + Pro Controller (WPAD) con detección en
  caliente; menú HOME del sistema vía ProcUI.
- **Controles implementado**: asignación de mandos a los 2 puertos del core
  (Mando 1/Mando 2) y remapeo estilo Our64 por acción del core, con múltiples
  botones por acción, `Y Add`, `X Unmap`, `- Reset`, captura con cuenta atrás y
  migración de `v1`/conf viejo. Sin verificar aún en hardware.
- SRAM automática al abrir/guardar/salir y cuatro slots con miniatura:
  implementados, pendientes de verificar en hardware (SAVE-1/2).
- Touch, búsqueda y descargas separadas de portadas/backdrops con progreso:
  implementados, pendientes de verificar en hardware (NAV-1/7, AS-1..4).
- Referencias visuales de UI en `ejemplos/` (raíz del proyecto).
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

0. el/los pendiente(s) de `PENDIENTES.md` que cubre la actividad (si no
   está registrado, agregarlo primero);
1. archivos que se crearán/modificarán;
2. explicación corta del cambio;
3. cómo se probará;
4. riesgos.

Esperar autorización. Después implementar en un cambio acotado y verificable.

Al terminar una actividad: si quedó probada en hardware, marcar su ID como
✅ en `PENDIENTES.md` (con fecha) y ajustar el resumen de la sección 11 si
cambia el estado general. Si no, dejarla 🔧/⬜ y documentar qué falta
verificar.
