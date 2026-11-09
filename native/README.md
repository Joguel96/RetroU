# App nativa Wii U — modo RPX estático

Frontend RetroU con **WUT + SDL2 + SDL2_ttf**. Cada build lleva **un core
Libretro enlazado estáticamente**, igual que RetroArch Wii U.

## Apps generadas

| App | Core | Sistema |
|---|---|---|
| `RetroU.wuhb` | ninguno (launcher) | detecta y lanza el RPX con Mocha |
| `RetroU-NES.wuhb` | fceumm | NES |
| `RetroU-SNES.wuhb` | snes9x | SNES |
| `RetroU-GBA.wuhb` | mgba | GBA |
| `RetroU-Test.wuhb` | test_core | prueba de vídeo |

Los RPX de core se copian a `retrou/cores/`:

```text
RetroU-NES.rpx
RetroU-SNES.rpx
RetroU-GBA.rpx
```

`RetroU.wuhb` se copia automáticamente a `dist/wiiu/apps/retrou/RetroU.wuhb`
(equivale a `sd:/wiiu/apps/retrou/RetroU.wuhb`).

El launcher escribe `launch/last.txt` y lanza el RPX del sistema con
**libmocha**:

```cpp
Mocha_InitLibrary();
Mocha_PrepareRPXLaunch(&load_info);
Mocha_LaunchHomebrewWrapperEx(&std_args);
```

Requiere `MochaPayload`/EnvironmentLoader activo en Aroma o Tiramisu.
El RPX destino lee `last.txt`, lo borra y carga la ROM con su core.

## Qué hace

- Escanea `sd:/retrou/roms/<Consola>/`.
- Muestra la UI de cristal con consolas, contadores y tarjetas.
- **A** carga la ROM con el core integrado en esa app.
- **Minus** abre el menú de RetroU.
- El render de la UI es bajo demanda: solo repinta cuando hay entrada o cambia
  el estado, no a 60 FPS constante.

No hay selector de cores: cada app es un sistema. Si abres una ROM de otro
sistema, avisa de que esa build no la soporta.

## Lo que falta

- Audio e input del core (fase 2).
- SRAM/savestates.
- Carátulas reales.
- Descarga/instalación de builds de cores.

## Build

Los cores se compilan fuera del árbol y se enlazan como `.a`:

```sh
native/cores/build-static-cores.sh
cd native
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Salidas:

```text
build/retrou_nes.rpx / .wuhb
build/retrou_snes.rpx / .wuhb
build/retrou_gba.rpx / .wuhb
build/retrou_test.rpx / .wuhb
```

## SD

```text
sd:/retrou/roms/NES/...
sd:/retrou/roms/SNES/...
sd:/retrou/roms/GBA/...
sd:/retrou/cores/system/
sd:/retrou/cores/savefiles/
```

## Logs

- `sd:/retrou/retrou_rpx.log`: carga/init/run del core estático.
- El RPL queda en la rama `experiment/rpl-loader`; no se compila en `main`.

## Input

| Mando | Acción |
|---|---|
| D-Pad / stick | mover selección |
| L / R | consola anterior / siguiente |
| A | cargar ROM con el core integrado |
| B | volver a "Todas" |
| X | detalle (placeholder) |
| Y | favorito (placeholder) |
| Start | test (solo build Test) |
| Minus | menú de RetroU en juego |
