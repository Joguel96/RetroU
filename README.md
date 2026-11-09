# RetroU

Frontend para Nintendo Wii U (Aroma) centrado en lanzar cores `.rpx` de
Libretro, organizar ROMs por consola y descargar carátulas. La identidad visual
es “glass caramel”: paneles de cristal, carátulas y navegación con GamePad.

## Estado

- **App nativa Wii U**: `native/` — esqueleto C++17 + WUT + SDL2/SDL2_ttf.
  Escanea `retrou/roms` y `retrou/cores`, pinta TV/GamePad y lanza el core con
  `_SYSLaunchTitleByPathFromLauncher`.
- **Interfaz React**: `web/` (Vite + React + Tailwind), extraída de
  `Joguel96/retroui`, sin backend ni Gemini, navegable solo con mando. Es la
  especificación visual y el banco de pruebas de la UI.
- **Referencia RetroArch**: `docs/retroarch-reference.md` (lanzamiento de
  `.rpx`, argv/HBL, sysapp, carátulas, ProcUI, WUHB).
- **Branding**: `assets/icon.png`, `assets/tv-splash.png`, `assets/drc-splash.png`.

## Estructura

```text
RetroU/
├── assets/                  icono y splashes (128x128, 1280x720, 854x480)
├── docs/
│   ├── retroarch-reference.md
│   └── ui.md
├── native/                  app Wii U: WUT + SDL2
│   ├── content/             Lato OFL (ui.ttf, ui-bold.ttf, OFL.txt)
│   └── src/main.cpp
├── web/                     interfaz React navegable con mando
└── README.md
```

## Compilar la app nativa

En MSYS2/devkitPro:

```sh
cd native
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Salidas:

```text
native/build/RetroU.elf
native/build/RetroU.rpx
native/build/RetroU.wuhb
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
sd:/wiiu/apps/RetroU/RetroU.wuhb
sd:/retrou/roms/<Consola>/*.rom
sd:/retrou/cores/*.rpx
sd:/retrou/thumbnails/<Sistema>/Named_Boxarts/*.png
sd:/retrou/launch/last.txt
```

## Hoja de ruta

1. ~~Esqueleto nativo + escaneo + lanzamiento de `.rpx`~~.
2. Validar core `.rpl` en proceso (fceumm) y frontend Libretro.
3. Vídeo/audio/input, SRAM y savestates dentro de RetroU.
4. Ajustes de memoria, escalado, widescreen y overrides.
5. Carátulas reales desde `thumbnails.libretro.com`.
6. Portar las vistas de `web/` a la UI nativa y probar en hardware.

## Documentación

- [Referencia RetroArch](docs/retroarch-reference.md)
- [Interfaz y mando](docs/ui.md)
- [Arquitectura y ajustes](docs/settings-roadmap.md)
- [App nativa](native/README.md)

## Guías externas

- [devkitPro Wii U examples](https://github.com/devkitPro/wiiu-examples)
- [WUT](https://github.com/devkitPro/wut)
- [SDL para devkitPro](https://github.com/devkitPro/sdl)
