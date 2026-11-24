# RetroU

Frontend propio para Nintendo Wii U, dirigido y publicado por **xoloDev**.
Cada consola tiene una aplicación independiente. NES, SNES y GBA enlazan
FCEUmm, Snes9x y mGBA mediante Libretro; N64 utiliza el motor Not64 + GLideN64
y audio AX adaptado a Wii U por Our64, integrado en el frontend de RetroU.

**Hecho con asistencia de IA (OpenAI Codex)** para código, depuración,
documentación y recursos gráficos. El mantenedor dirige el proyecto y realiza
las pruebas en hardware. Los motores pertenecen a sus respectivos autores:
véanse [créditos](CREDITS.md) y [licencias](LICENSES.md).

El código original de RetroU es **MIT**. Los componentes ajenos conservan sus
condiciones; **Snes9x restringe el uso comercial**. No se incluyen juegos ni BIOS.

## Descargar e instalar

Descarga el ZIP de [Releases](../../releases/latest). Incluye cuatro WUHB
para Aroma y cuatro RPX para Homebrew Launcher/Tiramisu, con créditos y
licencias. Copia la carpeta `wiiu` del entorno elegido a la raíz de la SD.
**Aroma y Tiramisu están probados por el mantenedor en Wii U física.**
Guía detallada y fuentes: [publicación y reconstrucción](docs/release.md).

Copia tus ROMs a estas rutas (los datos se conservan bajo `retrou`):

| App | Ruta en la SD | Formatos habituales |
|---|---|---|
| RetroU NES | `sd:/retrou/roms/nes/` | `.nes` |
| RetroU SNES | `sd:/retrou/roms/snes/` | `.sfc`, `.smc` |
| RetroU GBA | `sd:/retrou/roms/gba/` | `.gba` |
| RetroU N64 | `sd:/retrou/roms/n64/` | `.z64`, `.n64`, `.v64` |

Los cores NES/SNES/GBA se obtienen de sus repositorios de código y se compilan
como bibliotecas estáticas; sus URL y revisiones están en
`native/cores/build-static-cores.sh`. Los RPX del buildbot Libretro contienen
RetroArch más un core y no se utilizan como bibliotecas de RetroU.

## Estado

- **Apps nativas Wii U**: `native/` — NES (fceumm), SNES (snes9x) y GBA
  (mgba), compiladas como WUHB independientes con WUT/GX2.
- **Referencia RetroArch**: `docs/retroarch-reference.md` (historia técnica,
  sin usar RetroArch como base del frontend).
- **Branding**: `assets/icon.png`, `assets/tv-splash.png`, `assets/drc-splash.png`.
  Por consola: `assets/console/<consola>/` (generados con
  `tools/generate_console_assets.py`).
- **N64**: el frontend RetroU enlaza Not64 + GLideN64 + AX extraído de
  `C:/proyectos/Our64`. `bash native/build-retrou-n64.sh` genera
  `dist/wiiu/apps/RetroN64U/RetroN64U.wuhb`; la versión anterior con UI de
  Our64 ya no se compila. El mantenedor confirmó en Wii U el juego, controles,
  descargas, estados con miniaturas, HOME y reapertura de ROM en una sesión.
  El log de ejecución
  es `sd:/retrou/n64/retrou_n64.log`.
- **Mapeo de controles GX2**: editor estilo Our64 por acción del core, con varias
  asignaciones por acción, `Y Add`, `X Unmap`, `- Reset`, captura con cuenta atrás
  y persistencia explícita de acciones `none` en `control-map.conf` formato `v2`.
  El menú de
  partida permite ajustar cada ROM; hay tres perfiles por dispositivo y consola.
  HOME abre el menú rápido durante una ROM; desde la biblioteca y Ajustes
  abre el menú HOME de Wii U. El menú rápido ofrece «Cerrar aplicación».
- **Guardados y Assets**: cuatro slots de savestate con miniatura, SRAM al
  abrir/guardar/salir y descargas separadas de portadas y backdrops con
  progreso de imágenes realmente pendientes, conteo de fallos, búsqueda en el
  índice de nombres de Libretro, cola para solicitudes durante la actualización
  de metadata y actualización de portadas visibles al terminar.
  El usuario confirmó estas funciones y el cierre con descargas activas en
  hardware (AS-6).
- **Audio GBA**: mGBA genera muestras a 65 536 Hz. La conversión lineal a
  32 000 Hz de AUD-1 no eliminó el ruido en hardware. AUD-2 solicita una salida
  de 48 000 Hz y usa remuestreo sinc con paso bajo; el usuario confirmó que
  funciona en Wii U el 2026-11-20.
  El log registra la frecuencia y el formato obtenidos del dispositivo.
- **Salida de audio**: Sonido permite elegir frecuencia automática, 32 000 o
  48 000 Hz y búfer de 512, 1024 o 2048 muestras. Se guarda por consola en
  `sd:/retrou/config/<consola>/audio.conf` y se aplica al reiniciar esa app
  (AUD-3, confirmado en Wii U). La WUHB de N64 usa audio AX propio y no lee
  este archivo.
- **Salida de pantalla**: Vídeo permite elegir TV + GamePad, solo TV o solo
  GamePad en NES/SNES/GBA. Cada app guarda la elección en su `video.conf`;
  las configuraciones previas de tres valores conservan ambas pantallas.
  El usuario confirmó los tres modos en Wii U (UI-11).
- **Biblioteca GX2**: vistas cards, lista y carrusel; las dos últimas ofrecen
  filtro alfabético con Favoritos junto a «All», navegable con L/R. El carrusel NES presenta
  portadas verticales; SNES y N64 muestran las imágenes completas en el centro
  y los laterales. El usuario confirmó el carrusel N64 en Wii U.
  `Y` cambia de vista desde la biblioteca.
  En Lista, izquierda/derecha del D-pad o stick L avanza por páginas de cinco
  juegos conservando la fila seleccionada; arriba/abajo mueve un juego (NAV-11).
  Personalizar permite activar el fondo dinámico y elegir
  su espera (0, 1, 3, 5 o 10 s; 3 s iniciales). Las
  opciones de vista se guardan por consola en `view.conf`. Las vistas fueron
  confirmadas en hardware, incluida la nueva distribución de Ajustes (UI-2).

## Estructura

```text
RetroU/
├── assets/                  icono y splashes (128x128, 1280x720, 854x480)
├── docs/
│   ├── retroarch-reference.md
│   └── ui.md
├── native/                  apps Wii U: WUT + GX2 + cores estáticos
│   ├── content/             Lato OFL (ui.ttf, ui-bold.ttf, OFL.txt)
│   └── src/
└── README.md
```

## Compilar la app nativa

En MSYS2/devkitPro:

```sh
./native/build-retrou-gx2.sh
```

Salidas:

```text
dist/wiiu/apps/RetroNESU/RetroNESU.wuhb
dist/wiiu/apps/RetroSNESU/RetroSNESU.wuhb
dist/wiiu/apps/RetroGBAU/RetroGBAU.wuhb
dist/wiiu/apps/RetroN64U/RetroN64U.wuhb
```

## Layout de SD previsto

```text
sd:/wiiu/apps/RetroNESU/RetroNESU.wuhb
sd:/wiiu/apps/RetroSNESU/RetroSNESU.wuhb
sd:/wiiu/apps/RetroGBAU/RetroGBAU.wuhb
sd:/wiiu/apps/RetroN64U/RetroN64U.wuhb
sd:/retrou/roms/<consola>/
sd:/retrou/config/<consola>/
sd:/retrou/saves/<consola>/save/
sd:/retrou/snap/<consola>/
sd:/retrou/thumbnails/<consola>/
sd:/retrou/backdrops/<consola>/
sd:/retrou/roms/n64/
sd:/retrou/n64/                 configuración, arte, guardados y log N64
```

## Hoja de ruta

La futura ampliación abarca **todos los cores del catálogo Wii U de Libretro**,
incluidos MSX, PlayStation, Neo Geo y los demás, con integración en RetroU,
revisión individual de licencias y pruebas físicas. No están incluidos aún
en este release. El [inventario completo](docs/libretro-wiiu-roadmap.md)
distingue cores ya integrados y pendientes; varias variantes pueden emular
el mismo sistema. N64 mantiene su motor nativo procedente de Our64.
Los motores alternativos se incluirán en el RetroU de cada consola, con
selector de core y un solo motor activo; los savestates se separarán por motor
(CORE-2). El release inicial todavía contiene un motor por aplicación.
Los cores multisistema se agruparán por familias cuando tenga sentido, por
ejemplo **RetroU SEGA**, con los sistemas y capacidades de cada core indicados.

La lista canónica de tareas y su estado de verificación está en
[PENDIENTES.md](PENDIENTES.md). Una compilación correcta no equivale a una
prueba en hardware.

## Documentación

- [Referencia RetroArch](docs/retroarch-reference.md)
- [Interfaz y mando](docs/ui.md)
- [Arquitectura y ajustes](docs/settings-roadmap.md)
- [App nativa](native/README.md)
- [Validación en Wii U](docs/hardware-validation.md)

## Patrocinio

Puedes apoyar el desarrollo de RetroU mediante
[GitHub Sponsors de Joguel96](https://github.com/sponsors/Joguel96), el perfil
de patrocinio utilizado por xoloDev.

## Guías externas

- [devkitPro Wii U examples](https://github.com/devkitPro/wiiu-examples)
- [WUT](https://github.com/devkitPro/wut)
- [SDL para devkitPro](https://github.com/devkitPro/sdl)

## Verificación

El mantenedor confirmó las funciones de las cuatro apps en Wii U con Aroma,
incluidos HOME, reapertura N64, audio GBA y navegación por páginas (NAV-11).
El usuario confirmó la ronda final de las cuatro WUHB (DOC-2) y los cuatro
RPX en Tiramisu (REL-2) el 2026-11-23; véase [PENDIENTES.md](PENDIENTES.md).
