# App nativa Wii U

Frontend RetroU con **WUT + SDL2 + SDL2_ttf** y un **loader RPL propio** para
ejecutar cores dentro del proceso de RetroU.

## Qué hace

- Abre dos ventanas SDL2: TV (1280x720) y GamePad (854x480).
- Monta la SD y escanea `sd:/retrou/roms/<Consola>/`.
- Escanea `sd:/retrou/cores/*.rpl` y `*.rpx`.
- Muestra la UI de cristal con consolas, contadores y tarjetas.
- Navega con D-Pad/stick, A, B, X, Y, L/R.
- **A**:
  - si hay un core `.rpl` para el sistema, lo carga **dentro de RetroU** y
    ejecuta la ROM (RetroU no se cierra);
  - si solo hay `.rpx`, mantiene el fallback de lanzar la app del core.
- **Start** ejecuta `test.rpl`, un core sintético para validar el loader.
- **Minus** abre el menú de RetroU durante el core.

## Loader RPL

`RplLoader` lee el `.rpl` de `sd:/retrou/cores/`, descomprime sus secciones
(zlib), las mapea en un bloque de memoria, aplica las relocaciones
`ADDR32/ADDR16/REL24/GHS_REL16` y resuelve los imports débiles de newlib
(`__syscall_*`, `__register_frame_info`). Después busca los exports `retro_*`
en `.fexports`.

No usa `OSDynLoad_Acquire` para el core: Aroma solo resuelve módulos ya cargados
o RPLs del sistema, no un `.rpl` suelto de la SD.

## Cores RPL

- `sd:/retrou/cores/fceumm.rpl` (NES, generado con
  `native/cores/build-fceumm-rpl.sh`).
- `sd:/retrou/cores/test.rpl` (core de prueba que dibuja un frame de color).
- Los cores del buildbot son `.rpx` completos; no se pueden cargar en proceso.
  Hay que compilar cada core como `.rpl` con exports `retro_*`.

Ver [cores/README.md](cores/README.md).

## Qué falta

- Audio e input dentro del frontend Libretro (fase 2).
- Selector de ROM y guardado de SRAM/savestates.
- Carátulas reales.
- Portar todas las vistas de `web/`.

## Build

```sh
cd native
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Salidas:

```text
build/RetroU.elf
build/RetroU.rpx
build/RetroU.wuhb
```

El WUHB incluye las fuentes Lato OFL y el icono/splashes de `../assets/`.

## SD

```text
sd:/retrou/roms/NES/...
sd:/retrou/cores/fceumm.rpl
sd:/retrou/cores/test.rpl
sd:/retrou/cores/system/
sd:/retrou/cores/savefiles/
```

## Input

| Mando | Acción |
|---|---|
| D-Pad / stick | mover selección |
| L / R | consola anterior / siguiente |
| A | cargar core RPL + ROM, o lanzar `.rpx` |
| B | volver a "Todas" (catálogo) / cerrar menú en juego |
| X | detalle (placeholder) |
| Y | favorito (placeholder) |
| Start | ejecutar `test.rpl` |
| Minus (-) | abrir/cerrar el menú de RetroU durante un core |

En el menú de juego (Minus) el core queda pausado. Por ahora las opciones son
`Continuar` y `Cerrar juego y volver a RetroU`; savestates y remapeo quedan para
la siguiente fase.

`SDL_PollEvent` es quien bombea ProcUI en el port SDL2; no se llama a
`WHBProc` ni a `ProcUI` directamente.
