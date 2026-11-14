# Contrato del port N64 a RetroU

Fecha: 2026-11-13; alcance corregido por el usuario el 2026-11-14. Las
TASK-001–020 de `PLAN.md` produjeron una WUHB basada en la aplicación Our64,
pero **no son el port definitivo**. N64-2 sigue abierto. El usuario pide que
la biblioteca, descargas, ajustes, controles y UI sean los de RetroU; de
Our64 solo debe conservarse el motor de ejecución N64. AUD-2 (audio de GBA)
queda aplazado.

## Línea base verificable

| Elemento | Revisión o SHA-256 |
|---|---|
| RetroU antes de iniciar TASK-001 | `beeda766c26887245c05988bd7b7112ba3b6de53` |
| Our64 antes de iniciar TASK-001 | `2670a4cd4258e5dc848aa01322441c5c46a67061` |
| WUHB N64 distribuida y funcional en hardware | `3642b6f9c9429b8af456f8261bf4c38e84e315529f93edd24ab3ea1bab9cf283` |

Our64 **no estaba limpio** antes del port. Sus cambios previos no forman parte
de los commits de estas tareas. Identificadores de los archivos modificados:

| Archivo de Our64 | SHA-256 previo |
|---|---|
| `player/Makefile` | `FFE962743CC4C8FE34C617887D676FA9102B304C08647989D0487C69A2343854` |
| `player/source/video.cpp` | `0459513DCE692F8EEC915BD2176D9AF1ABEFE233BD9F7BA9468F4BD080F9CF77` |
| `third_party/gliden64/src/Graphics/GX2Context/gx2_GL.cpp` | `443A07EFE0C387934D03FA23B1E07EE12ED13728229931C5A55108DE0FA3D88A` |
| `third_party/libgui/source/drivers/wut/WutInputDriver.cpp` | `F9EB48EE98E99CAC7DE28353B45B7050165736341F83C1DF1EFDBE9E29771276` |
| `third_party/libgui/source/drivers/wut/WutThreadDriver.cpp` | `0A3B1F9DAE7C0344D809A1A95EF937ADA168A5187EB2A04097AAA4BA83D11ED6` |
| `tools/build-retrou-n64.sh` (archivo aún sin seguimiento) | `C5A162C86ADE28B917DBC4EA15094E9D7EFD2F84EBCF23D38DA227433E7AF76F` |

También existían `assets/` y `build.log` sin seguimiento en Our64. No se
incluirán por accidente en los commits. El build funcional depende de varios
de estos cambios previos; el hash de Git por sí solo **no** reproduce la WUHB
base. Cada build nuevo debe registrar la revisión, los cambios sucios y el
checksum de la WUHB.

## Frontera técnica de la WUHB provisional

- **Motor conservado:** recompiler y hardware N64 de Not64, GLideN64 para GX2,
  audio AX, pacing, hilos y lógica de estados/guardados de Our64. N64 no es un
  core Libretro en RetroU.
- **Capa a adaptar:** `ui.h` y `ui_draw.cpp` describen y dibujan las pantallas;
  `ui_wiiu.cpp` presenta cada una en el hilo gráfico mediante `video_call` y
  `ui_show`. `main.c`, `library_wiiu.c` y el input conectan esa UI con el motor.
- **Propiedad de GX2:** el hilo gráfico de Our64 sigue siendo el único dueño.
  La interfaz RetroU se integra a su puente sin iniciar otro renderizador GX2.
- **Binario:** una WUHB independiente llamada `RetroU N64`, ubicada en
  `dist/wiiu/apps/RetroN64U/RetroN64U.wuhb`. NES, SNES y GBA se compilan y se
  configuran por separado.

## Frontera definitiva solicitada

El RPX de Our64 compilado actualmente contiene `main`, biblioteca, menús,
descargador y motor N64 juntos. No se puede tratar ese archivo como un núcleo
aislado. El port definitivo requiere separar el motor Not64/GLideN64 y sus
servicios de ejecución de la aplicación Our64, y conectarlo a una WUHB cuyo
frontend sea el de `native/src/gx2/` de RetroU. La propiedad de GX2, el audio,
la entrada, los guardados y el ciclo de pausa deben tener un solo dueño.

Secuencia verificable:

1. Usar en `retrou_assets` el índice y las URL HTTPS de Libretro para N64,
   con `Named_Boxarts` y `Named_Snaps`, cancelación y actualización en vivo.
   Guardar en `sd:/retrou/thumbnails/n64` y `sd:/retrou/backdrops/n64`, como
   las demás consolas. No continuar el cliente `nn::ac` de Our64.
2. Definir una interfaz pequeña para iniciar, ejecutar, pausar y cerrar el
   motor N64 sin compilar `main.c`, `setup_wiiu.c`, `library_wiiu.c`,
   `covers_wiiu.c` ni la UI de Our64 dentro del frontend nuevo.
3. Resolver la propiedad única de GX2: adaptar el motor/GLideN64 al ciclo del
   renderizador RetroU y probar apertura y regreso repetidos de una ROM.
4. Conectar entrada de hasta cuatro jugadores, audio, SRAM y estados N64 al
   frontend RetroU sin leer ni sobrescribir los perfiles de otras consolas.
5. Compilar y distribuir `RetroN64U.wuhb` desde el frontend nuevo; verificar
   biblioteca, tres vistas, controles, ajustes, descargas y cierre primero en
   Cemu y después en Wii U física. Solo entonces cerrar N64-2/N64-5/N64-7.

Primer hito de extracción (2026-11-14):
`native/cores/build-n64-engine.sh` compila la revisión fija
`d3c56db253cd1b20987eb33ecd5797c60e527b64` en
`.cores/n64/not64_gliden64_wiiu.a`. El archivo contiene 161 objetos del motor
Not64/GLideN64 y servicios de audio y guardado; excluye el `main`,
biblioteca, ajustes, carátulas, menús, entrada, log, descargador, controlador
de estados y cliente Transfer Pak de Our64.
La entrada y los mensajes deben conectarse con RetroU. El log con auditoría de
miembros está en `native/build/logs/n64-engine-20261114-003411.log`.
SHA-256 del archivo:
`b2941c1c2d0c4be7b57edd142c07bd0feb9deea1a48f30438e2e01c303df6f92`.
Es una biblioteca de objetos, aún sin entrada ni puente al frontend RetroU;
no sustituye todavía a la WUHB N64 provisional.

La WUHB provisional permanece disponible para revertir y comparar el motor,
pero su UI y descargador no son la base del port definitivo.

## Datos y compatibilidad

| Dato | Ruta nueva | Ruta previa, solo como origen compatible |
|---|---|---|
| ROMs | `sd:/retrou/roms/n64/` | `sd:/wiiu/our64/roms/` |
| Ajustes y perfiles N64 | `sd:/retrou/n64/config/` | `sd:/wiiu/our64/config/` |
| Guardados de juego | `sd:/retrou/n64/saves/` | `sd:/wiiu/our64/saves/` |
| Estados y autosaves | `sd:/retrou/n64/states/` | `sd:/wiiu/our64/states/` |
| Portadas | `sd:/retrou/n64/art/` | `sd:/wiiu/our64/art/` |
| Backdrops | `sd:/retrou/n64/backdrops/` | No existía |
| Log N64 | `sd:/retrou/n64/retrou_n64.log` | `sd:/wiiu/our64/player-log.txt` |

Los datos previos no se eliminan ni se trasladan automáticamente. Las ROMs
antiguas se pueden leer sin copiarlas. Los guardados y estados nuevos se
escriben exclusivamente en RetroU; si falta un archivo nuevo, se admite la
lectura del anterior. Antes de cambiar una ruta, cada subsistema debe
comprobar tamaño de búfer, errores de SD y el resultado en el log.

## Procedencia y licencias

Our64 declara su código propio como GPL-2.0-or-later. Su Not64 vendorizado
proviene de `d35a7a1c454eca973995939dbca3f06725cf4a6e` (GPL-2.0), y
GLideN64 de `a04081ad6a966a9413bea63a55f502e0dc98bb18` (GPL-2.0,
con licencias adicionales en `licenses/`). `third_party/libgui/VENDORED.md`
registra el commit `03cb4283f30f1ff2557b4f34ee16aa076f5c0a8e` y su
README indica GPL sin precisar versión. Hay que conservar los avisos de cada
componente en cualquier distribución de fuente o binario.

## Verificación y reversión

Cada tarea modifica un archivo y termina con un commit descriptivo en el
repositorio que contiene ese archivo. El build deja log, hash de Git, estado
de cambios locales y SHA-256 de la WUHB. Después de cada ronda se conserva
la WUHB N64 anterior. Si la nueva falla, se restaura el binario cuyo SHA-256
consta arriba y se revierte únicamente el commit causal; no se hace `reset`
del árbol con cambios previos. Solo se marcará N64-2 como completado tras
validación explícita del usuario en Wii U.
