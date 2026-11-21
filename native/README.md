# App nativa Wii U — WUHB por consola

NES/SNES/GBA usan **WUT + SDL2 + SDL2_ttf** y un core Libretro estático por
WUHB. N64 es otra WUHB independiente: Not64 + GLideN64 + audio AX.

## Apps generadas

| App | Core | Sistema |
|---|---|---|
| `RetroNESU.wuhb` | fceumm | NES |
| `RetroSNESU.wuhb` | snes9x | SNES |
| `RetroGBAU.wuhb` | mgba | GBA |
| `RetroN64U.wuhb` | Not64 + GLideN64 + AX | N64 |

N64 se compila en el clon externo `C:/proyectos/Our64`, fuera de CMake y
`native/`. Los cambios del port están en el commit `869861d` y los ajustes
WUT 1.9.2 en `29de243`. La nueva WUHB arrancó en Cemu; falta la ronda N64
en Wii U y afinar la apariencia de Ajustes.

Los nombres visibles de las WUHB son `RetroU NES`, `RetroU SNES`,
`RetroU GBA` y `RetroU N64`. El nombre N64 se verificó en Cemu; falta
verificarlo en el menú de Wii U.

Distribución:

```text
dist/wiiu/apps/RetroNESU/RetroNESU.wuhb
dist/wiiu/apps/RetroSNESU/RetroSNESU.wuhb
dist/wiiu/apps/RetroGBAU/RetroGBAU.wuhb
dist/wiiu/apps/RetroN64U/RetroN64U.wuhb
```

No hay launcher, `RetroU.wuhb`, `retrou/cores` ni RPX sueltos.

## Datos

```text
sd:/retrou/roms/nes/
sd:/retrou/roms/snes/
sd:/retrou/roms/gba/
sd:/retrou/roms/n64/
sd:/retrou/n64/config/
sd:/retrou/n64/art/
sd:/retrou/n64/backdrops/
sd:/retrou/n64/saves/
sd:/retrou/n64/states/
sd:/retrou/n64/retrou_n64.log

sd:/retrou/config/theme.conf
sd:/retrou/config/menu-shortcut.conf
sd:/retrou/config/nes/nes.conf
sd:/retrou/config/nes/control-map.conf
sd:/retrou/config/nes/video.conf
sd:/retrou/config/nes/audio.conf
sd:/retrou/config/nes/view.conf
sd:/retrou/config/nes/profiles/
sd:/retrou/config/nes/rom-maps/
...
sd:/retrou/saves/<consola>/save/
sd:/retrou/snap/<consola>/
```

Cada app crea sus carpetas y archivos de configuración si no existen.
`control-map.conf` usa `v2`: `p1`/`p2` indican el slot asignado a cada puerto
(`p2` no se usa en GBA) y las secciones `[gamepad]`/`[pro]` guardan por acción
una o varias máscaras físicas en hexadecimal, o `none`. Los formatos `v1` y
anterior se leen y migran al guardar. Los tres perfiles por dispositivo se
guardan en `profiles/`; los mapas por ROM, en `rom-maps/`.

Cada app NES/SNES/GBA lee su propio `audio.conf` con formato `v1 <frecuencia>
<búfer>`: frecuencia `0` = Auto (GBA 48 000 Hz, NES/SNES 32 000 Hz), `1` =
32 000 Hz, `2` = 48 000 Hz; búfer `0` = 512, `1` = 1024, `2` = 2048 muestras.
Los cambios de Sonido se guardan enseguida y se aplican al próximo inicio,
porque el dispositivo SDL permanece abierto durante la sesión. Si falla la
frecuencia elegida, se prueba la otra; `retrou_rpx.log` registra la selección,
la frecuencia real y los fallos de apertura. N64 usa audio AX en otra WUHB y
no lee este `audio.conf`.
Vídeo añade salida TV + GamePad, solo TV o solo GamePad como cuarto valor
de `video.conf` en NES/SNES/GBA. Los archivos de tres valores siguen cargando
con ambas pantallas; la selección de salida aún requiere prueba en Wii U.

## Qué hace

- Escanea `sd:/retrou/roms/<consola>/`.
- Muestra la biblioteca en cards, lista o carrusel, sin barra de consolas.
- **A** carga la ROM con el core integrado.
- **Plus** abre Ajustes en dos paneles (secciones a la izquierda, opciones a
  la derecha); **Minus** abre el sidebar de filtros en cards.
- En lista y carrusel, **Minus** abre el buscador y **L/R** recorre «All»,
  la estrella de Favoritos, «#» y A–Z. La vista y el fondo dinámico se guardan por consola
  en `view.conf`, junto con la espera del backdrop (0, 1, 3, 5 o 10 s;
  3 s iniciales). **Y** cambia entre cards, lista y carrusel;
  **X** marca la ROM y su estrella se muestra en lista y carrusel.
- El carrusel NES muestra las portadas verticales; SNES y N64 ajustan las imágenes
  completas en el centro y en los laterales, sin recortarlas. N64 requiere
  comprobar el cambio visual en la WUHB de desarrollo.
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
- Al salir hacia el menú de Wii U, la app cancela las descargas pendientes,
  espera al hilo de red y registra `asset_shutdown_begin` y
  `asset_shutdown_complete` antes de `main_exit_clean`.
- Cards conserva un máximo de 16 texturas de portada y una sola portada de
  respaldo. GBA registra cada selección, memoria libre y render completado
  para localizar un posible cierre al recorrer la biblioteca.
- Los títulos recibidos en Latin-1 se muestran como UTF-8; portadas y fondos
  usan el mismo nombre de archivo ASCII seguro al guardar y al cargar.
- «Controles» en ese menú edita el mapa de botones de la ROM; la asignación
  de Mando 1/Mando 2 sigue siendo la global de Ajustes → Controles, incluso
  si un archivo por ROM contiene `p1`/`p2` antiguos. Al salir se restablece
  el mapa general. El archivo de la ROM se identifica con un hash estable de
  su ruta y se guarda en `rom-maps/`.
- La UI se dibuja en el bucle principal mientras la app está en primer plano.

No hay selector de cores: cada app es un sistema.

El port N64 está en extracción. `cores/build-n64-engine.sh` genera una
biblioteca estática de Not64/GLideN64 a partir de la revisión de Our64 fijada
en el propio script. Si CafeGLSL ya está instalado localmente, se puede indicar
`CAFEGLSL_SOURCE`; de otro modo el script usa el descargador verificado de
Our64. El motor de `.cores/n64/` se enlaza al target interno
`retrou_n64_gx2`; su WUHB de desarrollo se copia a una carpeta propia en
`dist` para pruebas y no sustituye `RetroN64U.wuhb`. El usuario confirmó que
una ROM inició y se pudo jugar en Wii U el 2026-11-18. El juego terminó con
resultado 0 al pulsar HOME, pero la interfaz se congeló después de restaurar
GX2. La espera de una nueva adquisición del primer plano no resolvió el
bloqueo en la prueba de Wii U del 2026-11-20. `cores/check-n64-bridge.sh`
verifica el enlace del motor con los adaptadores de video, cuatro mandos y
cuatro estados de RetroU, sin ejecutar una ROM. Véase
`docs/n64-port-contract.md`.

## Controles y verificación

- Mapeo de controles estilo Our64, con varias asignaciones por acción,
  `Y Add`, `X Unmap`, `- Reset`, captura con cuenta atrás y persistencia de
  acciones desasignadas en `control-map.conf`. NES y SNES muestran su imagen
  completa en el editor, GBA usa un solo puerto y las fuentes distinguen
  `Stick L` de `Stick R`.
- En Controles, los puertos aparecen como tarjetas: uno en GBA y dos en
  NES/SNES. Cada tarjeta muestra el dispositivo conectado o «Pulsa A para
  conectar». A sobre una tarjeta abre la captura de ese puerto; debajo están
  «Mapear botones», «Cambiar orden» y los atajos del menú por familia de mando.
  El touch del GamePad también permite seleccionar estas opciones.
  «Cambiar orden» recorre todos los puertos.
  La asignación se registra como `controller_connect_prompt` y
  `controller_order_saved`; aún requiere verificación en Wii U. Desde el editor,
  Plus abre tres perfiles por dispositivo: A carga el seleccionado, Y guarda el
  mapa actual.
- N64 Dev muestra cuatro tarjetas, asigna un mando nuevo al pulsar A sin abrir
  otra pantalla y mantiene el borde blanco del jugador al navegar sus acciones.
  Sus 18 acciones se guardan en `control-map.conf` y los cuatro puertos en
  `n64-ports.conf`; los mapas de GamePad y Pro/Classic se aplican al motor al
  iniciar una ROM. El usuario confirmó las tarjetas y el mapeo en Wii U el
  2026-11-20. «Cambiar orden» ahora cuenta los mandos conectados e ignora un
  segundo A del mismo mando; el usuario confirmó la corrección en Wii U.
- La asignación externa conserva un slot lógico cuando el Pro/Classic se
  desconecta; al reconectar en otro canal se intenta restaurar ese slot sin
  cambiar el mapa. El log registra `controller_disconnected`,
  `controller_rebound` y `controller_restored`.

El usuario confirmó en hardware las funciones de biblioteca, navegación,
guardados, mapeo y descargas de NES/SNES/GBA anteriores a AS-6, CTRL-8 y UI-2.
AS-6, CTRL-8, UI-2 y CTRL-9 se confirmaron en Wii U el 2026-11-13. La
conversión lineal de audio de GBA (AUD-1) se probó y no eliminó el ruido.
AUD-2 cambia la salida solicitada a 48 000 Hz y aplica un filtro sinc al
audio de mGBA de 65 536 Hz; el usuario confirmó el resultado en Wii U el
2026-11-20. El log
`retrou_rpx.log` registra `audio_opened`, `Audio sinc resampler` y los
reinicios de cola por atraso.

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

### Compilar el port N64 de desarrollo

Desde MSYS2/devkitPro:

```sh
bash native/build-retrou-n64-dev.sh
```

El script reconstruye el motor fijado, compila el frontend RetroU y copia
`native/build/retrou_n64_gx2.wuhb` a
`dist/wiiu/apps/RetroN64U-Dev/RetroN64U-Dev.wuhb`. Guarda logs en
`native/build/logs/` y registra el SHA-256 del binario copiado. La app lleva
el nombre `RetroU N64 Dev` y un Title ID distinto; todavía no está lista
para reemplazar la app N64 de uso normal. Al probarla, conservar
`sd:/retrou/retrou_rpx.log` y `sd:/retrou/n64/retrou_n64.log`.

### Compilar y recuperar la WUHB provisional de N64

Desde MSYS2/devkitPro, con el clon Our64 fijado en `29de243`:

```sh
cd /c/proyectos/Our64
bash tools/build-retrou-n64.sh /c/proyectos/RetroU
```

El script ejecuta `make -C player RELEASE=1`, deja la salida en
`player/release/our64.wuhb`, el log con commit y sumas SHA-256 en
`player/build-release/logs/build-<fecha>.log`, y copia de forma verificada
a `C:/proyectos/RetroU/dist/wiiu/apps/RetroN64U/RetroN64U.wuhb`.
Antes de sustituirla, copia la WUHB existente a `RetroN64U.previous.wuhb`.
El binario de esta ronda tiene SHA-256
`9d5a7dabeb0b079f3e14192bf84437cfef5450ca4889e37bba43c37ea9b3190b`;
el anterior, `5f822245644f1634f05ff982561383b8d0bc15068328219d45118ee072b42177`.
El log final de build es
`C:/proyectos/Our64/player/build-release/logs/build-20261113-212647.log`.

Si la nueva WUHB falla, cierra Cemu o RetroU N64, conserva el log
`sd:/retrou/n64/retrou_n64.log` y restaura
`dist/wiiu/apps/RetroN64U/RetroN64U.previous.wuhb` como
`RetroN64U.wuhb`; comprueba su SHA-256 antes de copiarla a la SD.
Para revertir código, usa `git revert` sobre el commit específico en
`C:/proyectos/Our64`, recompila y conserva el nuevo log. No borres datos
de `sd:/retrou/n64/` para volver a un binario anterior.

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
- `sd:/retrou/n64/retrou_n64.log`: ejecución N64, separada de NES/SNES/GBA.
- El log registra carga/restauración/guardado de mapas y perfiles, y un
  resumen de entrada por ROM. Los logs de compilación quedan en `native/build/logs/`.
- El log registra `menu_shortcut_loaded`, `menu_shortcut_saved`, fallos de
  lectura/escritura y `core_menu_shortcut_opened`; también `covers_refreshed`.
- `Audio reusing external device: <origen> Hz -> <salida> Hz` permite
  verificar la conversión de mGBA de 65 536 Hz a la frecuencia real del
  dispositivo compartido.
- Al cargar una ROM, `core_ports_global p1=... p2=...` permite comprobar qué
  asignación de mandos se aplica al core tras leer el mapa de esa ROM.
- La salida durante descargas registra `asset_shutdown_begin` y
  `asset_shutdown_complete` (y `GX2 asset: cancelado` si el trabajo ya
  comenzó). Un mando reconectado
  registra su slot y canal en `controller_rebound`/`controller_restored`.
- Si una máscara de `control-map.conf` no se puede leer, se usan los controles
  predeterminados y el log registra `control_map_parse_failed`.
- Las descargas consultan los índices de nombres de Libretro y escriben líneas
  `GX2 asset` de inicio, coincidencia, errores HTTP/red y fin en el mismo log.
  La comparación omite las etiquetas de región del título y prefiere la
  edición regional correspondiente cuando existe.
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
| Y en biblioteca | cambiar entre cards, lista y carrusel |
| Minus | filtros en cards; buscador en lista/carrusel |
| L/R | All, Favoritos, # y letras en lista/carrusel |
| Plus | Ajustes |
| Combinación configurada o Minus+Plus durante partida | menú de juego |

El menú durante el juego se abre con la combinación elegida para ese tipo de
mando o con Minus+Plus en el mismo mando. El touch del GamePad permite
seleccionar elementos visibles. Ver `PENDIENTES.md` para lo que falta validar.
