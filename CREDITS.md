# Créditos

## RetroU

Proyecto dirigido y publicado por **xoloDev**. RetroU aporta el frontend Wii U,
la biblioteca, navegación, configuración de mandos, integración de motores,
guardados, descargas y herramientas de empaquetado. El historial conserva las
aportaciones anteriores bajo Joguel96; el prototipo visual procede de
[Joguel96/retroui](https://github.com/Joguel96/retroui).

**Hecho con asistencia de inteligencia artificial:** se utilizó OpenAI Codex
para generar y modificar código, investigar integraciones, diagnosticar fallos,
preparar documentación y ayudar con recursos gráficos. La dirección del proyecto,
las decisiones y las pruebas en una Wii U física corresponden al mantenedor.
La IA no es autora de los motores de terceros ni sustituye sus atribuciones.

## Motores y API

| Proyecto | Autores y contribución |
|---|---|
| [Libretro](https://www.libretro.com/) | Libretro/RetroArch team y colaboradores: API, ports de cores, utilidades libretro-common e infraestructura |
| [FCEUmm](https://github.com/libretro/libretro-fceumm) | CaH4e3, Xodnizel, BERO y demás autores de FCE/FCE Ultra/FCEUmm; port Libretro y colaboradores: motor NES |
| [Snes9x](https://github.com/libretro/snes9x) | Gary Henderson, Jerremy Koot, Snes9x team y autores enumerados en su licencia; Hans-Kristian Arntzen, Daniel De Matteis y colaboradores del port Libretro: motor SNES |
| [mGBA](https://github.com/libretro/mgba) | endrift y colaboradores de mGBA/Libretro: motor GBA |
| [Our64](https://github.com/Ch0nbo/Our64) | Ch0nbo y colaboradores: adaptación nativa de N64 a Wii U, integración de CPU, gráficos y AX; fuente del motor extraído y referencia del ciclo de pausa/cierre y controles |
| [Not64](https://github.com/extremscorner/not64) | Extrems y colaboradores, sobre Wii64/Mupen64: CPU y recompilador PowerPC |
| [Wii64](https://github.com/emukidid/Wii64) / [Mupen64](https://github.com/mupen64plus) | Sus equipos y Hacktarux: bases de emulación y componentes heredados; mupen64plus aporta también datos por juego y decodificador JPEG usados por Our64 |
| [GLideN64](https://github.com/gonetz/GLideN64) | Sergey Lipskiy (Gonetz) y colaboradores; Orkin, autores de Glide64, z64, GLideHQ/GLideNHQ y demás créditos de su LICENSE: gráficos N64; backend GX2 de Our64 |
| [CafeGLSL](https://github.com/Exzap/CafeGLSL) | Exzap y colaboradores: compilación de shaders en Wii U, con sus avisos de dependencias |
| [cxd4 RSP](https://github.com/cxd4/rsp) | cxd4 y colaboradores: intérprete RSP |
| [miniLZO](https://www.oberhumer.com/opensource/lzo/) | Markus F. X. J. Oberhumer: compresión |

N64 **no usa Libretro**: RetroU enlaza Not64 + GLideN64 + AX extraídos de
Our64 dentro de su propio frontend. El código fuente de Our64 acompaña el
release para reconstruir el motor; sus menús originales no se compilan en RetroU.

Los cores NES/SNES/GBA se compilan desde sus repositorios de fuentes como
bibliotecas estáticas. No se extraen de los RPX del buildbot: esos RPX contienen
RetroArch más un core completo. RetroArch se consultó como referencia técnica;
la interfaz de RetroU es propia. Las utilidades libretro-common y `libretro.h`
realmente incorporadas conservan sus avisos MIT.

## Plataforma, bibliotecas y datos

- [devkitPro](https://devkitpro.org/), [WUT](https://github.com/devkitPro/wut)
  y sus colaboradores: toolchain y API Wii U.
- [SDL](https://www.libsdl.org/) y port Wii U de devkitPro; SDL_image,
  SDL_ttf, FreeType y HarfBuzz: audio, imágenes y texto.
- curl, Mbed TLS, libpng, zlib, bzip2, Brotli, libwebp y libjpeg-turbo:
  red, TLS y formatos de datos. Este software se basa parcialmente en el
  trabajo del Independent JPEG Group y utiliza FreeType Project.
  Portions of this software are copyright © 2025 The FreeType Project
  (https://freetype.org/). All rights reserved.
  This software is based in part on the work of the Independent JPEG Group.
- Łukasz Dziedzic y autores de Lato: tipografía, bajo SIL OFL.
- [libretro-thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails)
  y [libretro-database](https://github.com/libretro/libretro-database): índices,
  nombres y recursos consultados para portadas y metadata. Las ilustraciones
  originales de los juegos pertenecen a sus respectivos titulares.
- Equipos de [Aroma](https://aroma.foryour.cafe/),
  [Tiramisu](https://tiramisu.foryour.cafe/) y Homebrew Launcher: entornos y carga.
- Autores de React, Vite, Tailwind CSS y Lucide: prototipo web; sus licencias
  se conservan en sus paquetes. No se enlazan en los ejecutables Wii U.

Los autores adicionales y las dependencias vendorizadas se detallan en sus
fuentes y en `licenses/`. Esta lista no reemplaza ni borra esos avisos.
