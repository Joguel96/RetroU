# Distribución y reconstrucción

El release inicial es `v0.1.0`. Lo publica xoloDev; sus archivos incluyen:

- `RetroU-v0.1.0.zip`: cuatro WUHB, cuatro RPX, metadatos de Homebrew Launcher,
  guía de instalación, créditos y licencias.
- `RetroU-v0.1.0-sources.zip`: fuentes de RetroU y de los cuatro motores
  utilizados, sus modificaciones, parches y bibliotecas cubiertas por copyleft.
- `SHA256SUMS.txt`: sumas de integridad de ambos ZIP.

## Instalación

Extraer **solo la carpeta del entorno elegido**, copiando su carpeta `wiiu`
a la raíz de una SD FAT32. No copiar `Aroma` o `Tiramisu` literalmente a la SD.

- **Aroma:** usar `Aroma/wiiu/apps/`. Iniciar las WUHB desde el menú Wii U.
- **Tiramisu:** usar `Tiramisu/wiiu/apps/`. Iniciar los RPX desde Homebrew
  Launcher; cada app acompaña `meta.xml` e `icon.png`.

Los RPX se generan de los mismos targets que las WUHB y contienen sus motores
enlazados; no son cores para cargar desde otra app. Aroma ha sido probado por
el mantenedor. **Tiramisu queda pendiente de validación física (REL-2)**,
especialmente la gestión de HOME y el recompilador N64. Incluir el formato RPX
no constituye una garantía de compatibilidad probada.

Crear estas rutas y copiar allí los juegos propios:

```text
sd:/retrou/roms/nes/     .nes
sd:/retrou/roms/snes/    .sfc / .smc
sd:/retrou/roms/gba/     .gba
sd:/retrou/roms/n64/     .z64 / .n64 / .v64
```

La app crea las carpetas de configuración y guardados bajo `sd:/retrou/`.
Al actualizar, conservar esa carpeta. No se incluyen juegos ni BIOS.
HOME durante una partida abre RetroU; en biblioteca/Ajustes abre HOME de Wii U.

## Reconstrucción

Instalar devkitPro/devkitPPC, WUT y los portlibs Wii U/PowerPC indicados en
`build-manifest.json` dentro del paquete. Con MSYS2/bash:

```sh
bash native/cores/build-static-cores.sh
bash native/cores/build-n64-engine.sh
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake -S native -B native/build -DCMAKE_BUILD_TYPE=Release
cmake --build native/build --target stage_wuhb stage_n64 -j4
python tools/package_release.py --version v0.1.0
```

El ZIP completo de fuentes incluye `.cores/` sin los motores compilados.
Incluye aparte el SDK precompilado de CafeGLSL que se enlazó, con sus cabeceras
y licencias, para conservar exactamente esa dependencia de shaders.
Our64 se incluye con marcador de su revisión adaptada. Al clonar RetroU de
GitHub, el script obtiene la revisión pública `2670a4c` de Our64 y aplica
`native/cores/our64-retrou.patch`, que reproduce el snapshot local `d3c56db`.
Los parches gráficos y AX adicionales se aplican durante el build del motor.
Los cores Libretro se fijan a las revisiones del manifiesto. CafeGLSL se obtiene
con el descargador y la verificación SHA-256 heredados de Our64.

`tools/package_release.py` verifica los ocho ejecutables, crea los dos ZIP,
registra revisiones, hashes y versiones de portlibs y comprueba su contenido.
El empaquetado de fuentes parte de un commit limpio de RetroU y conserva los
archivos cubiertos por licencias ajenas. Los binarios nunca se comitean.
El empaquetador utiliza Python 3 y Pillow (también usado por las herramientas
gráficas del proyecto); genera iconos de Homebrew Launcher de 128×48 píxeles.

Las fuentes auxiliares de Mbed TLS y FreeType se guardan en
`native/build/release-dependencies/` antes de empaquetar. El manifiesto registra
su procedencia y hash. Las licencias completas acompañan ambos formatos.
Para reutilizar código o redistribuir binarios consultar [LICENSES.md](../LICENSES.md).
