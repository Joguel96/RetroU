# Cores para las apps RPX

`main` usa cores **estáticos**: cada app enlaza un `.a` y no hay loader RPL.

## Cores

| App | Core | Archive |
|---|---|---|
| NES | fceumm | `.cores/fceumm/fceumm_libretro_wiiu.a` |
| SNES | snes9x | `.cores/snes9x/libretro/snes9x_libretro_wiiu.a` |
| GBA | mgba | `.cores/mgba/build/mgba_libretro.a` |

## Generarlos

```sh
./build-static-cores.sh
```

El script clona en `.cores/` (ignorado por git), compila cada core para Wii U y
deja los `.a` que usa `native/CMakeLists.txt`.

## Test core

`test_core.cpp` / `test_core.def` es el core sintético de `RetroU-Test`; se
compila directamente en el RPX de prueba.

## RPL

El loader RPL y sus pruebas están en la rama:

```text
experiment/rpl-loader
```

No forma parte de las apps de `main`.
