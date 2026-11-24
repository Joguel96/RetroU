# libmocha vendored

- Origen: https://github.com/wiiu-env/libmocha
- Commit: `50fefdf`
- Licencia: LGPLv3 (`LICENSE`)
- Uso: lanzar el RPX de `RetroU` desde el launcher mediante
  `Mocha_PrepareRPXLaunch` + `Mocha_LaunchHomebrewWrapperEx`.

`include/` procede de ese commit. El archivo `lib/libmocha.a` usado por el
prototipo no se publica en el árbol actual y está ignorado por git.
Esta biblioteca no se enlaza en las cuatro apps actuales. Para retomar el
prototipo se debe compilar desde su fuente; el ZIP de fuentes del release
incluye la revisión `50fefdf` y se conserva LGPL v3.

Requiere `MochaPayload`/EnvironmentLoader activo en Aroma o Tiramisu.
