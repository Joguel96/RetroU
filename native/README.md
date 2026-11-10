# App nativa Wii U — WUHB por consola

Frontend RetroU con **WUT + SDL2 + SDL2_ttf**. Cada build lleva **un core
Libretro enlazado estáticamente** y se distribuye como WUHB independiente.

## Apps generadas

| App | Core | Sistema |
|---|---|---|
| `RetroNESU.wuhb` | fceumm | NES |
| `RetroSNESU.wuhb` | snes9x | SNES |
| `RetroGBAU.wuhb` | mgba | GBA |

Distribución:

```text
dist/wiiu/apps/RetroNESU/RetroNESU.wuhb
dist/wiiu/apps/RetroSNESU/RetroSNESU.wuhb
dist/wiiu/apps/RetroGBAU/RetroGBAU.wuhb
```

No hay launcher, `RetroU.wuhb`, `retrou/cores` ni RPX sueltos.

## Datos

```text
sd:/retrou/roms/nes/
sd:/retrou/roms/snes/
sd:/retrou/roms/gba/

sd:/retrou/config/theme.conf
sd:/retrou/config/nes/nes.conf
sd:/retrou/config/nes/control-map.conf
sd:/retrou/config/nes/video.conf
...
sd:/retrou/saves/<consola>/save/
sd:/retrou/snap/<consola>/
```

Cada app crea sus carpetas y archivos de configuración si no existen.

## Qué hace

- Escanea `sd:/retrou/roms/<consola>/`.
- Muestra solo la grilla de juegos, sin barra de consolas.
- **A** carga la ROM con el core integrado.
- **Plus** abre el sidebar con filtros, controles y ajustes.
- **Minus + Plus** abre el menú superpuesto durante el core.
- El render de la UI es bajo demanda: solo repinta cuando hay entrada o cambia
  el estado, no a 60 FPS constante.

No hay selector de cores: cada app es un sistema.

## Lo que falta

- Input del core.
- SRAM/savestates reales (las carpetas ya existen).
- Mapeo de controles y ajustes de vídeo funcionales.
- Carátulas reales.

El audio del core ya está conectado a SDL2.

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
build/retrou_nes.wuhb
build/retrou_snes.wuhb
build/retrou_gba.wuhb
```

## SD

```text
sd:/retrou/roms/nes/...
sd:/retrou/roms/snes/...
sd:/retrou/roms/gba/...
sd:/retrou/config/<consola>/
sd:/retrou/saves/<consola>/save/
sd:/retrou/snap/<consola>/
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
