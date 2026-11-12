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
sd:/retrou/config/menu-shortcut.conf
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
- En lista y carrusel, **Minus** abre el buscador y **L/R** recorre «All»,
  la estrella de Favoritos, «#» y A–Z. La vista y el fondo dinámico se guardan por consola
  en `view.conf`, junto con la espera del backdrop (0, 1, 3, 5 o 10 s;
  3 s iniciales). **Y** también alterna Favoritos;
  **X** marca la ROM y su estrella se muestra en lista y carrusel.
- El carrusel NES muestra las portadas verticales; el SNES ajusta las imágenes
  completas en el centro y en los laterales, sin recortarlas.
- El touch del GamePad selecciona juegos y opciones en la UI.
- En Ajustes → Controles se elige la combinación de apertura del menú por
  GamePad, Pro/Classic y Wii Remote/Nunchuk. Es global para NES, SNES y GBA;
  se guarda en `sd:/retrou/config/menu-shortcut.conf` (`v1`). **Minus + Plus**
  siempre abre el menú como acceso de emergencia desde el mismo mando.
- Las portadas de la biblioteca se recargan al completar una descarga, sin
  reiniciar la app.
- En «Solo faltantes», la barra cuenta solo imágenes aún ausentes en la SD;
  muestra intentos procesados y fallos por separado, sin sumar metadata.
- Las solicitudes manuales hechas mientras se actualiza metadata quedan en
  cola y comienzan al terminar el trabajo actual; Assets indica «En cola».
- Cards conserva un máximo de 16 texturas de portada y una sola portada de
  respaldo. GBA registra cada selección, memoria libre y render completado
  para localizar un posible cierre al recorrer la biblioteca.
- Los títulos recibidos en Latin-1 se muestran como UTF-8; portadas y fondos
  usan el mismo nombre de archivo ASCII seguro al guardar y al cargar.
- «Controles» en ese menú edita el mapa de la ROM; al salir se restablece el
  mapa general. El archivo de la ROM se identifica con un hash estable de su
  ruta y se guarda en `rom-maps/`.
- La UI se dibuja en el bucle principal mientras la app está en primer plano.

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
- El log registra `menu_shortcut_loaded`, `menu_shortcut_saved`, fallos de
  lectura/escritura y `core_menu_shortcut_opened`; también `covers_refreshed`.
- Si una máscara de `control-map.conf` no se puede leer, se usan los controles
  predeterminados y el log registra `control_map_parse_failed`.
- Las descargas consultan los índices de nombres de Libretro y escriben líneas
  `GX2 asset` de inicio, coincidencia, errores HTTP/red y fin en el mismo log.
  Usan el método directo si no hay índice y conservan el archivo anterior
  cuando falla un reemplazo forzado.
- `main_exit_clean` distingue el cierre normal de una interrupción; en GBA,
  `gba_cards_focus`, `gba_cover_load`/`gba_cover_ready` y
  `gba_cards_rendered` acotan la última selección al recorrer Cards.
- El RPL queda en la rama `experiment/rpl-loader`; no se compila en `main`.

## Input

| Mando | Acción |
|---|---|
| D-Pad / Stick L | mover selección |
| A | abrir juego u opción |
| B | volver un nivel; borrar en buscador |
| X | favorito en biblioteca; aceptar en buscador |
| Y en lista/carrusel | mostrar u ocultar Favoritos |
| Minus | filtros en cards; buscador en lista/carrusel |
| L/R | All, Favoritos, # y letras en lista/carrusel |
| Plus | Ajustes |
| Combinación configurada o Minus+Plus durante partida | menú de juego |

El menú durante el juego se abre con la combinación elegida para ese tipo de
mando o con Minus+Plus en el mismo mando. El touch del GamePad permite
seleccionar elementos visibles. Ver `PENDIENTES.md` para lo que falta validar.
