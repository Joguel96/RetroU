# App nativa Wii U

Esqueleto funcional de RetroU usando **WUT + SDL2 + SDL2_ttf**.

## Qué hace

- Abre dos ventanas SDL2: TV (1280x720) y GamePad (854x480).
- Monta la SD y escanea `sd:/retrou/roms/<Consola>/`.
- Escanea `sd:/retrou/cores/*.rpx`.
- Muestra una UI de cristal con las 9 consolas, contadores y tarjetas.
- Navega con D-Pad / stick, A, B, X, Y, L/R.
- Al pulsar A escribe `sd:/retrou/launch/last.txt` y lanza el core `.rpx`
  correspondiente con `_SYSLaunchTitleByPathFromLauncher`.

## Qué falta

- Paso de ROM al core. `_SYSLaunchTitleByPathFromLauncher` no acepta argv; el
  core arranca standalone. Para lanzar core + ROM hay que implementar el camino
  HBL/argv de `docs/retroarch-reference.md` o parchear el core.
- Carátulas reales: ahora se dibujan portadas placeholder con iniciales.
- Persistencia de favoritos, ajustes y último juego.
- Portar el resto de vistas de `web/`.

## Build

En MSYS2/devkitPro:

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

El WUHB incluye `content/ui.ttf`, `content/ui-bold.ttf` y `content/OFL.txt`
(Lato, SIL OFL), además del icono y los splashes de `../assets/`.

## SD

```text
sd:/retrou/roms/NES/...
sd:/retrou/roms/SNES/...
sd:/retrou/cores/fceumm.rpx
sd:/retrou/cores/snes9x_libretro.rpx
sd:/retrou/launch/last.txt
```

El mapeo de core por sistema está en `kConsoles` dentro de `src/main.cpp`.

## Input

| Mando | Acción |
|---|---|
| D-Pad / stick | mover selección |
| L / R | consola anterior / siguiente |
| A | lanzar juego / core |
| B | volver a "Todas" |
| X | detalle (placeholder) |
| Y | favorito (placeholder) |
| Start | detalle (placeholder) |

`SDL_PollEvent` es quien bombea ProcUI en el port SDL2; no se llama a
`WHBProc` ni a `ProcUI` directamente.
