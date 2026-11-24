# Licencias de RetroU y sus componentes

El código original del frontend, herramientas y documentación de RetroU se
publica bajo [MIT](LICENSE), por xoloDev y sus colaboradores. Se permite usarlo,
estudiarlo, modificarlo y redistribuirlo, también comercialmente, conservando
el aviso de licencia. Esto solo concede derechos sobre las aportaciones propias.

Los motores y archivos derivados o incorporados de terceros conservan sus
licencias, autores y avisos. La licencia MIT de la raíz no los sustituye.
Los adaptadores de `native/src/n64/` que integran estructuras y servicios de
Our64/Not64, y los parches de ese motor, se distribuyen bajo GPL-2.0-or-later;
se conservan las condiciones específicas de GLideN64 y demás archivos originales.

| Distribución | Componentes principales | Condiciones relevantes |
|---|---|---|
| RetroU NES | FCEUmm/FCE Ultra | GPL v2 o posterior según los archivos; el binario combinado se distribuye conforme a GPL v2 |
| RetroU SNES | Snes9x y su port Libretro | Licencia Snes9x: distribución y uso no comercial; no se concede permiso comercial sobre el motor |
| RetroU GBA | mGBA | MPL 2.0; se entregan las fuentes de los archivos cubiertos y sus modificaciones |
| RetroU N64 | Our64, Not64/Wii64/Mupen64, GLideN64, miniLZO | GPL v2, conservando las opciones de versiones posteriores donde existan |

Las bibliotecas enlazadas mantienen sus licencias: Libretro API/libretro-common
(MIT según los archivos usados), SDL2/SDL2_image/SDL2_ttf (zlib), WUT (zlib),
CafeGLSL (MIT y avisos de sus dependencias), cxd4 (CC0), curl, zlib, libpng,
libjpeg-turbo, HarfBuzz, Brotli, bzip2 y libwebp (sus respectivas licencias).
FreeType se usa bajo GPL v2 en NES/N64 y bajo FreeType License en SNES/GBA.
Este producto utiliza FreeType, desarrollado por el FreeType Project
(https://freetype.org/). Para Mbed TLS 2.28.8 se elige GPL-2.0-or-later en
NES/N64 y Apache-2.0 en SNES/GBA, según su licencia dual original.
Las fuentes Lato conservan SIL Open Font License 1.1.
La libmocha histórica del prototipo experimental conserva LGPL v3; no se
enlaza en las cuatro apps actuales. Sus fuentes se incluyen en el paquete
correspondiente y su archivo compilado no forma parte del árbol actual.

`licenses/` contiene copias de los avisos y textos obtenidos de las versiones
utilizadas. El ZIP del release los acompaña; el ZIP de fuentes contiene el
frontend, las fuentes exactas de los cuatro motores, sus subcomponentes,
los parches y los scripts de construcción. El SDK precompilado CafeGLSL se
incluye con sus avisos y hash. Las fuentes adicionales de Mbed TLS
y FreeType acompañan ese ZIP. Las versiones del SDK y portlibs se registran
en el manifiesto de construcción; devkitPro/WUT se instala aparte.

Para reconstruir se necesita el ZIP de fuentes completo del mismo release,
no solo el archivo automático «Source code» de GitHub, que omite `.cores/`.
Véase [publicación y compilación](docs/release.md).

Los juegos, BIOS, portadas descargadas, marcas y material de referencia tienen
sus propios titulares. No se incluyen ROMs comerciales ni BIOS. Las portadas
se consultan en los servicios de Libretro y no se relicencian como RetroU.
Las capturas de terceros en `ejemplos/` son referencias, no material MIT.

La aplicación completa no debe describirse como íntegramente MIT ni como
libre de restricciones comerciales: en particular, el motor Snes9x conserva
su condición no comercial. Los avisos originales prevalecen en cada archivo.
