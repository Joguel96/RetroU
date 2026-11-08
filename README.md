# RetroU

Frontend para Nintendo Wii U (Aroma) centrado en lanzar cores `.rpx` de
Libretro, organizar ROMs por consola y descargar carátulas. La identidad visual
es “glass caramel”: paneles de cristal, carátulas y navegación con GamePad.

## Estado

- **Interfaz React**: `web/` (Vite + React + Tailwind), extraída de
  `Joguel96/retroui`, sin backend ni Gemini, navegable solo con mando.
- **Referencia RetroArch**: `docs/retroarch-reference.md` (lanzamiento de
  `.rpx`, argv/HBL, sysapp, carátulas, ProcUI, WUHB).
- **App nativa Wii U**: pendiente de portar la interfaz a C++/WUT/SDL2 usando
  `docs/retroarch-reference.md` y los ejemplos de devkitPro.
- **Branding**: `assets/icon.png`, `assets/tv-splash.png`, `assets/drc-splash.png`.

## Estructura

```text
RetroU/
├── assets/                  icono y splashes (128x128, 1280x720, 854x480)
├── docs/
│   ├── retroarch-reference.md
│   └── ui.md
├── web/                     interfaz React navegable con mando
└── README.md
```

## Ejecutar la interfaz

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
sd:/retrou/retrou.cfg
```

## Hoja de ruta

1. Portar las vistas de `web/` a la app nativa Wii U (SDL2 + GX2).
2. Implementar el lanzador de `.rpx` (sysapp/HBL + argv) descrito en la
   referencia.
3. Escanear `sd:/retrou/roms` y persistir el catálogo.
4. Descargar carátulas a `sd:/retrou/thumbnails`.
5. Empaquetar `RetroU.wuhb` con `wuhbtool`.

## Documentación

- [Referencia RetroArch](docs/retroarch-reference.md)
- [Interfaz y mando](docs/ui.md)

## Guías externas

- [devkitPro Wii U examples](https://github.com/devkitPro/wiiu-examples)
- [WUT](https://github.com/devkitPro/wut)
- [SDL para devkitPro](https://github.com/devkitPro/sdl)
