# RetroU

Frontend propio para Nintendo Wii U (Aroma). Cada consola tiene una WUHB
independiente. NES, SNES y GBA enlazan un core Libretro estático; N64 usa
Not64, GLideN64 y audio AX. La UI, los datos y los guardados son de RetroU.

## Estado

- **Apps nativas Wii U**: `native/` — NES (fceumm), SNES (snes9x) y GBA
  (mgba), compiladas como WUHB independientes con WUT/GX2.
- **Interfaz React**: `web/` (Vite + React + Tailwind), extraída de
  `Joguel96/retroui`, sin backend ni Gemini, navegable solo con mando. Es la
  especificación visual y el banco de pruebas de la UI.
- **Referencia RetroArch**: `docs/retroarch-reference.md` (historia técnica,
  sin usar RetroArch como base del frontend).
- **Branding**: `assets/icon.png`, `assets/tv-splash.png`, `assets/drc-splash.png`.
  Por consola: `assets/console/<consola>/` (generados con
  `tools/generate_console_assets.py`).
- **N64**: el frontend RetroU enlaza Not64 + GLideN64 + AX extraído de
  `C:/proyectos/Our64`. `bash native/build-retrou-n64.sh` genera
  `dist/wiiu/apps/RetroN64U/RetroN64U.wuhb`; la versión anterior con UI de
  Our64 ya no se compila. El usuario probó una ROM, controles y descargas en
  Wii U. El menú de partida recién integrado compila, pero falta probarlo en
  hardware junto con HOME y guardados (N64-2/7/10/11). El log de ejecución
  es `sd:/retrou/n64/retrou_n64.log`.
- **Mapeo de controles GX2**: editor estilo Our64 por acción del core, con varias
  asignaciones por acción, `Y Add`, `X Unmap`, `- Reset`, captura con cuenta atrás
  y persistencia explícita de acciones `none` en `control-map.conf` formato `v2`.
  El menú de
  partida permite ajustar cada ROM; hay tres perfiles por dispositivo y consola.
  En Ajustes → Controles se elige por familia de mando cómo abrir el menú;
  `Minus + Plus` sigue disponible en todos.
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
  La nueva opción requiere verificación en Wii U.
- **Biblioteca GX2**: vistas cards, lista y carrusel; las dos últimas ofrecen
  filtro alfabético con Favoritos junto a «All», navegable con L/R. El carrusel NES presenta
  portadas verticales; SNES y N64 muestran las imágenes completas en el centro
  y los laterales. El cambio N64 falta comprobarlo visualmente.
  `Y` cambia de vista desde la biblioteca.
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
├── web/                     interfaz React navegable con mando
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

## Ejecutar la interfaz React

```sh
cd web
npm install
npm run dev
```

- Gamepad por defecto.
- Teclado/ratón solo para desarrollo: `http://localhost:3000/?keyboard=1`.

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

La lista canónica de tareas y su estado de verificación está en
[PENDIENTES.md](PENDIENTES.md). Una compilación correcta no equivale a una
prueba en hardware.

## Documentación

- [Referencia RetroArch](docs/retroarch-reference.md)
- [Interfaz y mando](docs/ui.md)
- [Arquitectura y ajustes](docs/settings-roadmap.md)
- [App nativa](native/README.md)
- [Validación en Wii U](docs/hardware-validation.md)

## Guías externas

- [devkitPro Wii U examples](https://github.com/devkitPro/wiiu-examples)
- [WUT](https://github.com/devkitPro/wut)
- [SDL para devkitPro](https://github.com/devkitPro/sdl)
