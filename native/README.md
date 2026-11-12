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
sd:/retrou/config/nes/view.conf
sd:/retrou/config/nes/profiles/
sd:/retrou/config/nes/rom-maps/
...
sd:/retrou/saves/<consola>/save/
sd:/retrou/snap/<consola>/
```

Cada app crea sus carpetas y archivos de configuración si no existen.

## Qué hace

- Escanea `sd:/retrou/roms/<consola>/`.
- Muestra la biblioteca en cards, lista o carrusel, sin barra de consolas.
- **A** carga la ROM con el core integrado.
- **Plus** abre Ajustes; **Minus** abre el sidebar de filtros en cards.
- En lista y carrusel, **Minus** abre el buscador y **L/R** recorre el filtro
  alfabético. La vista y la casilla de fondo dinámico se guardan por consola
  en `view.conf`.
- El touch del GamePad selecciona juegos y opciones en la UI.
- **Minus + Plus** abre el menú superpuesto durante el core.
- «Controles» en ese menú edita el mapa de la ROM; al salir se restablece el
  mapa general. El archivo de la ROM se identifica con un hash estable de su
  ruta y se guarda en `rom-maps/`.
- El render de la UI es bajo demanda: solo repinta cuando hay entrada o cambia
  el estado, no a 60 FPS constante.

No hay selector de cores: cada app es un sistema.

## Lo que falta

- Verificar en hardware los cuatro slots de savestate con miniatura y la
  persistencia de SRAM al cargar, guardar y salir.
- Mapeo de controles estilo Our64 funcional, con varias asignaciones por acción,
  `Y Add`, `X Unmap`, `- Reset`, captura con cuenta atrás y persistencia de
  acciones desasignadas en `control-map.conf`. NES y SNES muestran su imagen
  completa en el editor, GBA usa un solo puerto y las fuentes distinguen
  `Stick L` de `Stick R`.
- En Controles, «Cambiar orden de mandos» espera A en el mando físico para
  cada puerto. Desde el editor, Plus abre tres perfiles por dispositivo:
  A carga el seleccionado, Y guarda el mapa actual.
- Verificar en hardware la descarga separada de portadas/backdrops, metadatos
  automáticos y barra de progreso.

El input básico del core ya está conectado: A/B/X/Y/L/R, Start/Select y D-Pad/stick.

## Build

Los cores se compilan fuera del árbol y se enlazan como `.a`:

```sh
native/cores/build-static-cores.sh
cd native
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Para compilar las variantes GX2 y copiarlas automáticamente a `dist/wiiu/apps/`:

```sh
./native/build-retrou-gx2.sh
```

El script acepta `BUILD_TYPE`, `JOBS`, `DEVKITPRO` y `CMAKE_WIIU` como
variables de entorno. Cada ejecución conserva un log en `native/build/logs/`.

Salidas:

```text
native/build/retrou_nes_gx2.wuhb
native/build/retrou_snes_gx2.wuhb
native/build/retrou_gba_gx2.wuhb
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

- `sd:/retrou/retrou_rpx.log`: arranque, carga y ejecución del core estático;
  las tres ejecuciones anteriores quedan en `.1`, `.2` y `.3`.
- El log registra carga/restauración/guardado de mapas y perfiles, y un
  resumen de entrada por ROM. Los logs de compilación quedan en `native/build/logs/`.
- El RPL queda en la rama `experiment/rpl-loader`; no se compila en `main`.

## Input

| Mando | Acción |
|---|---|
| D-Pad / Stick L | mover selección |
| A | abrir juego u opción |
| B | volver un nivel; borrar en buscador |
| X | favorito en biblioteca; aceptar en buscador |
| Minus | filtros en cards; buscador en lista/carrusel |
| L/R | filtro alfabético en lista/carrusel |
| Plus | Ajustes |

El menú durante el juego se abre con Minus+Plus. El touch del GamePad permite
seleccionar elementos visibles. Ver `PENDIENTES.md` para lo que falta validar.
| Y | favorito (placeholder) |
| Start | test (solo build Test) |
| Minus | menú de RetroU en juego |
