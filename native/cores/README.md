# Cores RPL para RetroU

Esta carpeta contiene el frontend Libretro mínimo y dos cores de prueba:

- `test_core.cpp` / `test_core.def`: core sintético que genera un frame de color.
  Sirve para validar el loader RPL propio, los exports `retro_*` y el pipeline
  de vídeo sin depender de una ROM.
- `fceumm_rpl/`: empaqueta `fceumm_libretro_wiiu.a` como `.rpl`.

## Generar el core de prueba

```sh
cd native/cores
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
cp build/test_core.rpl ../../dist/retrou/cores/test.rpl
```

## Generar fceumm.rpl

```sh
./build-fceumm-rpl.sh
```

El script clona `libretro-fceumm` en `.spike_fceumm` (ignorado por git), compila
la librería estática para Wii U y la enlaza como `.rpl` con exports `retro_*`.
El resultado se copia a `dist/retrou/cores/fceumm.rpl`.

## Cómo lo carga RetroU

- `RplLoader` lee el `.rpl` de la SD.
- Descomprime `.text`, `.rodata`, `.data`, `.bss` y `.fexports`.
- Aplica las relocaciones y resuelve imports débiles de newlib.
- Busca `retro_init`, `retro_run`, etc. en `.fexports`.
- `retro_run()` corre en el bucle de RetroU; `video_refresh` actualiza una
  textura SDL que se pinta en TV y GamePad.

No usa `OSDynLoad_Acquire`: Aroma no carga un `.rpl` suelto de la SD; solo
resuelve módulos ya cargados o RPLs del sistema.
