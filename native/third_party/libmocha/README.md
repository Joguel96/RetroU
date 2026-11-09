# libmocha vendored

- Origen: https://github.com/wiiu-env/libmocha
- Commit: `50fefdf`
- Licencia: LGPLv3 (`LICENSE`)
- Uso: lanzar el RPX de `RetroU` desde el launcher mediante
  `Mocha_PrepareRPXLaunch` + `Mocha_LaunchHomebrewWrapperEx`.

`include/` y `lib/libmocha.a` se copiaron del build de ese commit con el
toolchain de devkitPro. Si se actualiza libmocha, recompilar y reemplazar
ambos.

Requiere `MochaPayload`/EnvironmentLoader activo en Aroma o Tiramisu.
